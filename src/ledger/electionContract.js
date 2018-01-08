var validatorSetsSize       = 5;
var numberOfVotesPassed     = 0.5;
var effectiveVotingInterval = 24 * 60 * 60 * 1000 * 1000; //3 min

var proposalVar    = 'proposal';
var proofVar       = 'proof';
var ballotVar      = 'ballot';
var abolishVar     = 'abolishValidator';
var nothingVar     = 'none';
var startTimeVar   = 'votingStartTime';
var expiredTimeVar = 'votingExpiredTime';
var initiatorVar   = 'votingInitiator';

var equitiesVar    = 'equityStructure';
var currentSetsVar = 'currentSets';

function main(input_str)
{
    var result = false;
	var input = JSON.parse(input_str);
    switch(input.type)
    {
        case 1://pledgor applys to become validator
	    	callBackLog(sender + ' apply to become validator. ');
	    	result = ApplyAsValidator();
            break;
        case 2://validator applys to exit validators sets
	    	callBackLog(sender + ' quit validator identity. ');
	    	result = QuitValidatorIdentity();
            break;
        case 3://someone applys to abolish certain validator
	    	callBackLog(sender + ' apply to abolish validator: ' + input.arg1);
	    	result = AbolishValidator(input.arg1, input.arg2); //arg1-address, arg2-proof
            break;
        case 4://quit abolish certain validator
	    	callBackLog(sender + ' quit to abolish validator.');
	    	result = QuitAbolishValidator();
            break;
        case 5://vote for cancelCertainValidator or quitCancelCertainValidator
	    	callBackLog(sender + ' votes for abolish validator: ' + input.arg1);
	    	result = VoteAbolishValidator(input.arg1);//agreeOrReject
            break;
        case 6: //query interface
	    	callBackLog(sender + ' query the info of ' + input.arg1);
            result = Query(input.arg1);
            break;
        default:
	    	callBackLog('unidentified operation type.');
            break;
    }

    return result;
}

function ApplyAsValidator()
{
    var currentSets = callBackGetValidators();
    if(currentSets === false)
    {
        callBackLog('Get current validators failed.');
        return false;
    }

    var pledgors = GetPledgors(currentSets);

    if (pledgors[sender] === undefined)
        pledgors[sender] = payCoinAmount;
    else
        pledgors[sender] += payCoinAmount;

    if(false === SetEquityStructure(pledgors))
        return false;

    return SetValidatorSets(pledgors);
}

function QuitValidatorIdentity()
{
    var currentValidators = callBackGetValidators();
    if(currentValidators === false)
    {
        callBackLog('Get current validator sets failed.'); 
        return false;
    }

    if(currentValidators.includes(sender) === false)
    {
        callBackLog('current validator sets has no ' + sender); 
        return false;
    }

    var pledgors = GetPledgors(currentValidators);
    if(pledgors[sender] != 0) 
    {
        var result = callBackPayCoin(sender, pledgors[sender]);
        if(result === false)
        {
            callBackLog('Return all pledge coin(' + pledgors[sender] + ') to ' + sender + ' failed.');
            return false;
        }
    }

    delete pledgors[sender];
    if(false === SetEquityStructure(pledgors))
        return false;

    return SetValidatorSets(pledgors);
}

function AbolishValidator(nodeAddr, proof)
{
    var currentValidators = callBackGetValidators();
    if(currentValidators === false)
    {
        callBackLog('Get current validator sets failed.'); 
        return false;
    }

    if(currentValidators.includes(nodeAddr) === false)
    {
        callBackLog('current validator sets has no ' + nodeAddr); 
        return false;
    }

    var proposal = callBackGetAccountMetaData(thisAddress, proposalVar);
    if(proposal != false)
    {
        if(proposal.value != nothingVar)
        {
            proposalContent = JSON.parse(proposal.value);
            var expiredTime = proposalContent[expiredTimeVar];
            var now         = consensusValue.close_time;

            if(now <= expiredTime)
            {
                callBackLog('There is unfinished vote proposal, please submit after that.'); 
                return false;
            }
        }
    }

    var votingStartTime = consensusValue.close_time;

    var newProposal = {};
    newProposal[abolishVar]     = nodeAddr;
    newProposal[proofVar]       = proof;
    newProposal[startTimeVar]   = votingStartTime;
    newProposal[expiredTimeVar] = votingStartTime + effectiveVotingInterval;
    newProposal[initiatorVar]   = sender;

    var proposalStr = JSON.stringify(newProposal);
    var proposalObj = {'key': proposalVar, 'value': proposalStr};

    var ret = callBackSetAccountMetaData(proposalObj);
    if(ret === false)
        callBackLog('Set proposal that to be voted in metadata( ' + proposalStr + ') failed.');

    return ret;
}

function QuitAbolishValidator()
{
    var proposal = callBackGetAccountMetaData(thisAddress, proposalVar);
    if(proposal === false)
    {
        callBackLog('Get proposal failed, maybe no one had applied to ablish validator.'); 
        return true;
    }

    if(proposal.value === nothingVar)
    {
        callBackLog('Proposal is none, it should be finished or quit already.'); 
        return true;
    }

    var proposalContent = JSON.parse(proposal.value);
    if(sender != proposalContent[initiatorVar])
    {
        callBackLog(sender + 'is not initiator, has no permission to quit the proposal.');
        return false; 
    }

    return ClearAllProposalContent();
}

function VoteAbolishValidator(voteChoice) //voteChoice: agree-1 or disagree-any number
{
    if(false === checkEffectiveVotingTime())
        return false;

    var AbolishAddr = GetTobeAbolishedValidator();
    if(AbolishAddr === false || AbolishAddr === true) //else return valid address to be abolished
        return AbolishAddr;

    var currentSets = callBackGetValidators();
    if(currentSets === false)
    {
        callBackLog('Get current validator sets failed.'); 
        return false;
    }

    if(currentSets.includes(AbolishAddr) === false)
    {
        callBackLog('Tere is no ' + AbolishAddr + ' in current validator sets.'); 
        return true;
    }

    if(currentSets.includes(sender) === false)
    {
        callBackLog(sender + ' has no permission to vote.'); 
        return true;
    }

    var voteResult = VoteProcess(currentSets, voteChoice);
    callBackLog('voteResult: ' + voteResult); 

    if(voteResult === false)
        return false;

    if(voteResult === 'unfinished')
        return true;

    if(voteResult === 'through')
    {
        var pledgors = GetPledgors(currentSets);
        var result = callBackPayCoin(AbolishAddr, pledgors[AbolishAddr]);
        if(result === false)
        {
            callBackLog('Return all pledge coin(' + pledgors[AbolishAddr] + ') to ' + AbolishAddr + ' failed.');
            return false;
        }

        delete pledgors[AbolishAddr];
        if(false === SetEquityStructure(pledgors))
            return false;

        SetValidatorSets(pledgors);
    }

    return ClearAllProposalContent();
}

function Query(arg)
{
    var result = callBackContractQuery(thisAddress, arg);
    return result;
}

function query(arg)
{
    var result = {};
    var ope    = parseInt(arg);

    switch(ope)
    {
        case 1:
            var currentSets = callBackGetValidators();
            result['Current_validators_sets'] = currentSets;
            break;
        case 2:
            GetEquityStructure(result);
            break;
        case 3:
            GetProposalContent(result);
            break;
        case 4:
            GetBallotContent(result);
            break;
        default:
            var currentSets = callBackGetValidators();
            result['Current_validators_sets'] = currentSets;
            GetEquityStructure(result);
            GetProposalContent(result);
            GetBallotContent(result);
            break;
    }

    callBackLog('query result:');
    callBackLog(result);

    return result;
}

function GetProposalContent(result)
{
    var proposalData = callBackGetAccountMetaData(thisAddress, proposalVar);
    if(proposalData != false)
    {
        var proposal = {};
        if(proposalData.value != nothingVar)
        {
            var proposalContent = JSON.parse(proposalData.value);
            proposal['Initiator']                 = proposalContent[initiatorVar];
            proposal['Ablish_reason']             = proposalContent[proofVar];
            proposal['Voting_start_time']         = proposalContent[startTimeVar];
            proposal['Voting_expired_time']       = proposalContent[expiredTimeVar];
            proposal['To_be_abolished_validator'] = proposalContent[abolishVar];
        }
        else
        {
            proposal = nothingVar;
        }

        result['Proposal_content_about_ablish_validator'] = proposal;
    }
}

function GetBallotContent(result)
{
    var ballotData = callBackGetAccountMetaData(thisAddress, ballotVar);
    if(ballotData != false)
    {
        ballot = {};
        if(ballotData.value != nothingVar)
        {
            ballotContent = JSON.parse(ballotData.value);
            for(vote in ballotContent)
            {
                if(ballotContent[vote] === 1)
                    ballot[vote] = 'Agree';
                else
                    ballot[vote] = 'Disagree';
            }
        }
        else
            var ballot = nothingVar;

       result['Current_voting_result'] = ballot;
    }
}

function GetEquityStructure(result)
{
    var equitiesData = callBackGetAccountMetaData(thisAddress, equitiesVar);
    if(equitiesData != false)
    {
        var equityStruct = JSON.parse(equitiesData.value);
        var equities     = SortDictionary(equityStruct);
        result['Current_pledgors_and_pledge_coin'] = equities;
    }
}

function VoteProcess(currentSets, voteChoice)
{
    var ballotDict = {};

    var ballot = callBackGetAccountMetaData(thisAddress, ballotVar);
    if(ballot != false)
        if(ballot.value != nothingVar)
            ballotDict = JSON.parse(ballot.value);

    ballotDict[sender] = voteChoice;

    agreeCnt = 0;
    for(vote in ballotDict)
    {
        if(ballotDict[vote] === 1)
            agreeCnt++;
    }

    var voteCnt    = Object.getOwnPropertyNames(ballotDict).length;
    var currentCnt = currentSets.length;

    if(agreeCnt / currentCnt >= numberOfVotesPassed) //proposal through
        return 'through';

    if((voteCnt - agreeCnt) / currentCnt > (1 - numberOfVotesPassed))//proposal rejected
        return 'rejected'; 

    if((agreeCnt / currentCnt < numberOfVotesPassed) && (voteCnt - agreeCnt) / currentCnt <= (1 - numberOfVotesPassed))
    {
        var voteDictStr = JSON.stringify(ballotDict);
        var newBallot   = {'key': ballotVar, 'value': voteDictStr};
        var ret = callBackSetAccountMetaData(newBallot);
        if(ret === false)
        {
            callBackLog('Set ballot(' + voteDictStr + ') failed.'); 
            return false;
        }

        return 'unfinished';
    }
}

function GetPledgors(currentSets)
{
    var pledgors = {};
    var equities = callBackGetAccountMetaData(thisAddress, equitiesVar);
    if(equities === false)
    {
        callBackLog('Get ' + equitiesVar + ' from metadata failed, then set it.'); 
        for(index in currentSets)
            pledgors[currentSets[index]] = 0;
    }
    else
    {
        pledgors = JSON.parse(equities.value);
    }

    return pledgors;
}

function SetEquityStructure(pledgors)
{
    var newEquities = {};
    var pledgorsStr = JSON.stringify(pledgors);
    var newEquities = {'key': equitiesVar, 'value': pledgorsStr};

    var result = callBackSetAccountMetaData(newEquities);
    if(result === false)
        callBackLog('Set ' + equitiesVar + ' in metadata failed.');

    return result;
}

function SetValidatorSets(pledgors)
{
    var sortedPledgors = SortDictionary(pledgors);
    var newSets = TopNKeyInMap(sortedPledgors, validatorSetsSize);
    var setsStr = JSON.stringify(newSets);

    result = callBackSetValidators(setsStr);
    if(result === false)
        callBackLog('Set validator sets failed.');

    return result;
}

function GetTobeAbolishedValidator()
{
    var proposal = callBackGetAccountMetaData(thisAddress, proposalVar);
    if(proposal === false)
    {
        callBackLog('Get proposal that to be voted failed.'); 
        return false;
    }

    if(proposal.value === nothingVar)
    {
        callBackLog('Vote maybe finished or proposal was quit.'); 
        return true;
    }

    var proposalContent = JSON.parse(proposal.value);
    var AbolishAddr     = proposalContent[abolishVar];
    if(AbolishAddr === undefined)
    {
        callBackLog('There is no validator that to be abolished in proposal.'); 
        return false;
    }

    return AbolishAddr;
}

function checkEffectiveVotingTime()
{
    var proposal = callBackGetAccountMetaData(thisAddress, proposalVar);
    if(proposal === false)
    {
        callBackLog('Get proposal failed.');
        return false;
    }

    if(proposal.value === nothingVar)
    {
        callBackLog('Vote maybe finished or proposal was quit.'); 
        return true;
    }

    var proposalContent   = JSON.parse(proposal.value);
    var votingExpiredTime = proposalContent[expiredTimeVar];
    var now               = consensusValue.close_time;

    if(now > votingExpiredTime)
    {
        callBackLog('Voting time expired.'); 
        ClearAllProposalContent();
        return false;
    }
    
    return true;
}

function ClearAllProposalContent()
{
    //clear proposal and ballot
    var noneProposal = {'key': proposalVar, 'value': nothingVar};
    proposalRet = callBackSetAccountMetaData(noneProposal);
    callBackLog('Set proposal none: ' + proposalRet);
    
    var noneBallot = {'key': ballotVar, 'value': nothingVar};
    ballotRet = callBackSetAccountMetaData(noneBallot);
    callBackLog('Set ballot none: ' + ballotRet);

    return proposalRet && ballotRet;
}

function by(name, minor)
{
    return function(x,y)
    {
        var a,b;
        if(x && y && typeof x === 'object' && typeof y ==='object')
        {
            a = x[name];
            b = y[name];

            if(a === b)
                return typeof minor === 'function' ? minor(y, x) : 0;

            if(typeof a === typeof b)
                return a > b ? -1:1;

            return typeof a > typeof b ? -1 : 1;
        }
    }
}

function SortDictionary(Dict) //value: desc sort first, key: asce sort second
{
    var tmpArr = [];
    for (key in Dict) 
        tmpArr.push({ 'key': key, 'value': Dict[key] });

    tmpArr.sort(by('value', by('key')));

    var sortMap = {};
    for (var i = 0; i < tmpArr.length; i++) 
    {
        var key = tmpArr[i]['key'];
        var val = tmpArr[i]['value'];
        sortMap[key] = val;
    }

    Dict = sortMap;

    return sortMap;
}

function TopNKeyInMap(sortedMap, n)
{
    var cnt = 0;
    var topN = [];
    for (key in sortedMap)
    {
        topN[cnt] = key;
        cnt++;

        if (cnt >= n)
            break;
    }

    return topN;
}
