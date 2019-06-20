'use strict';


const send_proposal = 'send_proposal_';
const receive_proposal = 'receive_proposal_';

const send_relay = 'send_relay';
const receive_relay = 'receive_relay';

const vote_rate = 0.7;
const notary_list = ['a001ce7acd9c7cb6b9bd6acaeecaaa0d7ff240241a9d15', 'a001e5c2e5142c2b57e72dd5783b383eabfdf4f4e4f6bb'];

const EXECUTE_STATE_INITIAL = 1;
const EXECUTE_STATE_PROCESSING = 2;
const EXECUTE_STATE_FAIL = 3;
const EXECUTE_STATE_SUCCESS = 4;

function findListIndex(array, key){
    let i = 0;
    for (i = 0; i < array.length; i += 1) {
        if (array[i] === key) {
            return i;
        }
    }
    return false;
}

function doIssue(operation_address,from,to,seq,amount){
    let args = {
        'action':'issue',
        'from': from,
        'to': to,
        'cpcSeq':seq,
        'amount': amount
    };
    let transaction = {
        'operations': [{
            'type': 3,
            'payment':{
                'dest_address':operation_address,
                'input':JSON.stringify(args)
            }
        }]
    };
    
    let isSuccess = callBackDoOperation(transaction);
    if (!isSuccess) {
        //callBackLog('payment error: '+ JSON.stringify(transaction));
        return false;
    }
}

function doTransfer(operation_address, from, to, amount) {
    let args = {
        'action': 'transfer',
        'from': from,
        'to': to,
        'amount': amount
    };
    let transaction = {
        'operations': [{
            'type': 3,
            'payment': {
                'dest_address': operation_address,
                'input': JSON.stringify(args)
            }
        }]
    };

    let isSuccess = callBackDoOperation(transaction);
    if (!isSuccess) {
        callBackLog('payment error: ' + JSON.stringify(transaction));
        return false;
    }
}


//initial relay
function initRelayContract(input){
    let send_relay_data = {
        'init_seq': 0,
        'complete_seq': 0,
        'notary_list': notary_list,
        'f_comm_addr': thisAddress,
        't_comm_addr': input.t_comm_addr,
        'f_chain_id': 'CHAIN_20190528_A',
        't_chain_id':'CHAIN_20190528_B'
    };

    let receive_relay_data = {
        'init_seq': 0,
        'complete_seq': 0,
        'notary_list': notary_list,
        'chain_id': 'CHAIN_20190528_A'
    };

    let send_relay_db = callBackGetAccountMetaData(thisAddress, send_relay);
    let receive_relay_db = callBackGetAccountMetaData(thisAddress, receive_relay);
    if (send_relay_db === false&&receive_relay_db===false) {
        let transaction = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': send_relay,
                    'value': JSON.stringify(send_relay_data)
                }
            },{
                'type': 4,
                'set_metadata': {
                    'key': receive_relay,
                    'value': JSON.stringify(receive_relay_data)
                }
            }]
        };

        let flag = callBackDoOperation(transaction);
        if (!flag) {
            throw 'Relay contract execution initialization failed';
        }
    }
}

//send cross chain tx
function checkSendOperationTx(input){
    let metadata = callBackGetAccountMetaData(input.f_assets_addr,'tx_'+input.seq);
    let tx_proof = {};
    if (metadata) {
        tx_proof = JSON.parse(metadata.value);
    }

    let flag = tx_proof.from === input.from && tx_proof.to === thisAddress && tx_proof.amount === input.amount&&tx_proof.seq===input.seq;
    if (flag === false) {
        throw 'The operation contract does not cover this transaction';
    }
}

function checkSendProposalTx(input){
    let send_proposal_seq = callBackGetAccountMetaData(thisAddress,send_proposal+input.f_assets_addr +'_'+input.seq);
    if(send_proposal_seq!==false){
        throw 'Submit duplicate transaction';
    }
}

function sendCrossChain(input){
    checkSendProposalTx(input);
    checkSendOperationTx(input);
    
    let send_relay_db = callBackGetAccountMetaData(thisAddress, send_relay);
    let send_relay_data = JSON.parse(send_relay_db.value);
    let send_proposal_seq = send_relay_data.init_seq + 1;

    let proposal = {
        'seq': send_proposal_seq,
        'amount': input.amount,
        'from': input.from,
        'to': input.to,
        'f_assets_addr': input.f_assets_addr,
        't_assets_addr': input.t_assets_addr,
        'f_comm_addr': send_relay_data.f_comm_addr,
        't_comm_addr': send_relay_data.t_comm_addr,
        'f_chain_id': send_relay_data.f_chain_id,
        't_chain_id': send_relay_data.t_chain_id
    };

    let proposal_state = {
        'proposal': proposal,
        'state': EXECUTE_STATE_INITIAL,
        'vote': [],
        'vote_count': 0
    };

    let relay_data = {
        'init_seq': send_proposal_seq,
        'complete_seq': send_relay_data.complete_seq,
        'notary_list': send_relay_data.notary_list,
        'f_comm_addr': send_relay_data.f_comm_addr,
        't_comm_addr': send_relay_data.t_comm_addr,
        'f_chain_id': send_relay_data.f_chain_id,
        't_chain_id':send_relay_data.t_chain_id
    };

    let transaction = {
        'operations': [{
            'type': 4,
            'set_metadata': {
                'key': send_proposal + send_proposal_seq,
                'value': JSON.stringify(proposal_state)
            }
        },
        {
            'type': 4,
            'set_metadata': {
                'key': send_relay,
                'value': JSON.stringify(relay_data)
            }
        },
        {
            'type': 4,
            'set_metadata': {
                'key': input.f_assets_addr +'_'+input.seq,
                'value':send_proposal_seq
            }
        }]
    };

    let isSuccess = callBackDoOperation(transaction);
    if (!isSuccess) {
        throw 'Contract execution cross chain transfer initialization failed';
    }

}

//proccess send cross tx
function checkSendNotary(send_relay_db,send_proposal_db,input) {
    let complete_seq = send_relay_db.complete_seq + 1;
    let state = EXECUTE_STATE_INITIAL;
    let vote_array = [];
    let i = 0;
    for (i = 0; i < send_proposal_db.vote.length; i += 1) {
        if (sender === send_proposal_db.vote[i][0]) {
            throw 'A notary public may vote on each proposal only once';
        }
    }
    for (i = 0; i < send_proposal_db.vote.length; i += 1) {
        vote_array.push([send_proposal_db.vote[i][0], send_proposal_db.vote[i][1]]);
        if (input.state !== send_proposal_db.vote[i][1]) {
            state = EXECUTE_STATE_FAIL;
            break;
        }
    }

    vote_array.push([sender, input.state]);
    if (state === EXECUTE_STATE_FAIL) {

        let send_proposal_info = {
            'proposal': send_proposal_db.proposal,
            'state': EXECUTE_STATE_FAIL,
            'vote': vote_array,
            'vote_count': vote_array.length
        };

        doTransfer(send_proposal_db.proposal.f_assets_addr, thisAddress, send_proposal_db.proposal.from, send_proposal_db.proposal.amount);
        let send_relay_info = {
            'init_seq': send_relay_db.init_seq,
            'complete_seq': complete_seq,
            'notary_list': send_relay_db.notary_list,
            'f_comm_addr': send_relay_db.f_comm_addr,
            't_comm_addr': send_relay_db.t_comm_addr,
            'f_chain_id': send_relay_db.f_chain_id,
            't_chain_id':send_relay_db.t_chain_id
        };

        let transaction = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': send_proposal + input.seq,
                    'value': JSON.stringify(send_proposal_info)
                }
            },
            {
                'type': 4,
                'set_metadata': {
                    'key': send_relay,
                    'value': JSON.stringify(send_relay_info)
                }
            }]
        };

        let isSuccess = callBackDoOperation(transaction);
        if (!isSuccess) {
            throw 'Contract failure';
        }
        callBackLog(JSON.stringify(transaction));
        return true;
    }
}

function handleSendElect(send_relay_db,send_proposal_db,input){ 
    let vote_array = [];
    let i = 0;
    for (i = 0; i < send_proposal_db.vote.length; i += 1) {
        vote_array.push([send_proposal_db.vote[i][0], send_proposal_db.vote[i][1]]);
    }
    vote_array.push([sender, input.state]);

    let send_proposal_info = {
        'proposal': send_proposal_db.proposal,
        'state': EXECUTE_STATE_FAIL,
        'vote': vote_array,
        'vote_count': vote_array.length
    };

    if (parseInt(send_proposal_info.vote_count) < parseInt(send_relay_db.notary_list.length * vote_rate + 0.5)) {
        send_proposal_info.state = EXECUTE_STATE_PROCESSING;
        let tx = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': send_proposal + input.seq,
                    'value': JSON.stringify(send_proposal_info)
                }
            }]
        };
        let isflag = callBackDoOperation(tx);
        if (!isflag) {
            throw 'Contract failure';
        }
        return;
    }

    let send_relay_info = {
        'init_seq': send_relay_db.init_seq,
        'complete_seq': send_relay_db.complete_seq+1,
        'notary_list': send_relay_db.notary_list,
        'f_comm_addr': send_relay_db.f_comm_addr,
        't_comm_addr': send_relay_db.t_comm_addr,
        'f_chain_id': send_relay_db.f_chain_id,
        't_chain_id':send_relay_db.t_chain_id
    };

    switch (input.state) {
    case EXECUTE_STATE_FAIL:
        send_proposal_info.state = EXECUTE_STATE_FAIL;
        doTransfer(send_proposal_db.proposal.f_assets_addr, thisAddress, send_proposal_db.proposal.from, send_proposal_db.proposal.amount);
        break;
    case EXECUTE_STATE_SUCCESS:
        send_proposal_info.state = EXECUTE_STATE_SUCCESS;
        break;
    default:
        throw 'Return result exception';
    }

    let transaction = {
        'operations': [{
            'type': 4,
            'set_metadata': {
                'key': send_proposal + input.seq,
                'value': JSON.stringify(send_proposal_info)
            }
        },
        {
            'type': 4,
            'set_metadata': {
                'key': send_relay,
                'value': JSON.stringify(send_relay_info)
            }
        }]
    };

    let isSuccess = callBackDoOperation(transaction);
    if (!isSuccess) {
        throw 'Contract failure';
    }
    callBackLog(JSON.stringify(send_proposal_info));
}

function onSendProposalEvent(input) {

    let relay = callBackGetAccountMetaData(thisAddress, send_relay);
    if (relay === false) {
        throw 'Channel information does not exist';
    }
    let send_relay_db = JSON.parse(relay.value);
    let complete_seq = send_relay_db.complete_seq + 1;

    if (findListIndex(send_relay_db.notary_list, sender) === false) {
        throw 'The operator is not a notary public';
    }

    if (input.seq !== complete_seq) {
        throw 'Inconsistent timing of communication information processing';
    }

    let proposal = callBackGetAccountMetaData(thisAddress, send_proposal + input.seq);
    if (proposal === false) {
        throw 'The proposal does not exist';
    }

    let send_proposal_db = JSON.parse(proposal.value);
    if (checkSendNotary(send_relay_db,send_proposal_db,input) === true) {
        return;
    }

    handleSendElect(send_relay_db,send_proposal_db,input);
}


//handle receive cross tx
function initReceiveProposalEvent(receive_relay_db,proposal,input){
    let proposal_vote = [];
  
    let origin_proposal = [sender,proposal.seq,proposal.amount,proposal.from,proposal.to,proposal.f_assets_addr,proposal.t_assets_addr,proposal.f_comm_addr,proposal.t_comm_addr,proposal.f_chain_id,proposal.t_chain_id];

    proposal_vote.push(origin_proposal);

    let receive_proposal_info = {
        'proposals': proposal_vote,
        'state': EXECUTE_STATE_INITIAL,
        'vote_count': proposal_vote.length
    };

    let receive_relay_info = {
        'init_seq': receive_relay_db.init_seq+1,
        'complete_seq': receive_relay_db.complete_seq,
        'notary_list': receive_relay_db.notary_list,
        'chain_id': receive_relay_db.chain_id
    };

    let transaction = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': receive_proposal + input.seq,
                    'value': JSON.stringify(receive_proposal_info)
                }
            },
            {
                'type': 4,
                'set_metadata': {
                    'key': receive_relay,
                    'value': JSON.stringify(receive_relay_info)
                }
            }]
        };

        let isSuccess = callBackDoOperation(transaction);
        if (!isSuccess) {
            throw 'Contract failure';
        }
}


function checkRecceiveNotary(receive_relay_db,receive_proposal_db,proposal,input){
    let proposal_vote = [];
    let state = EXECUTE_STATE_INITIAL;

    proposal_vote = receive_proposal_db.proposals;
    let i = 0;
    for (i = 0; i < receive_proposal_db.proposals.length; i += 1) {
        if (sender === receive_proposal_db.proposals[i][0]) {
            throw 'A notary public may vote on each proposal only once';
        }
    }

    for (i = 0; i < receive_proposal_db.proposals.length; i += 1) {
        let flag = input.seq!==receive_proposal_db.proposals[i][1]||input.amount!==receive_proposal_db.proposals[i][2]||
        input.from !==receive_proposal_db.proposals[i][3]||input.to!==receive_proposal_db.proposals[i][4]||
        input.f_assets_addr!==receive_proposal_db.proposals[i][5]||input.t_assets_addr!==receive_proposal_db.proposals[i][6]||
        input.f_comm_addr!==receive_proposal_db.proposals[i][7]||input.t_comm_addr!==receive_proposal_db.proposals[i][8]||
        input.f_chain_id!==receive_proposal_db.proposals[i][9]||input.t_chain_id!==receive_proposal_db.proposals[i][10];
        if (flag===true) {
            state = EXECUTE_STATE_FAIL;
            break;
        }
    }

    let origin_proposal = [sender,proposal.seq,proposal.amount,proposal.from,proposal.to,proposal.f_assets_addr,proposal.t_assets_addr,proposal.f_comm_addr,proposal.t_comm_addr,proposal.f_chain_id,proposal.t_chain_id];
    proposal_vote.push(origin_proposal);

    let receive_proposal_info = {
        'proposals': proposal_vote,
        'state': state,
        'vote_count': proposal_vote.length
    };

    if(state === EXECUTE_STATE_INITIAL){
        return false;
    }

    let receive_relay_info = {
        'init_seq': receive_relay_db.init_seq,
        'complete_seq': receive_relay_db.complete_seq+1,
        'notary_list': receive_relay_db.notary_list,
        'chain_id': receive_relay_db.chain_id
    };

    let transaction = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': receive_proposal + input.seq,
                    'value': JSON.stringify(receive_proposal_info)
                }
            },
            {
                'type': 4,
                'set_metadata': {
                    'key': receive_relay,
                    'value': JSON.stringify(receive_relay_info)
                }
            }]
        };
    
        let isSuccess = callBackDoOperation(transaction);
        if (!isSuccess) {
            throw 'Contract failure';
        }
}

function handleReceiveElect(receive_relay_db,receive_proposal_db,proposal,input){
    let proposal_vote = [];

    let state = EXECUTE_STATE_INITIAL;
    proposal_vote = receive_proposal_db.proposals;
       
    let origin_proposal = [sender,proposal.seq,proposal.amount,proposal.from,proposal.to,proposal.f_assets_addr,proposal.t_assets_addr,proposal.f_comm_addr,proposal.t_comm_addr,proposal.f_chain_id,proposal.t_chain_id];
    proposal_vote.push(origin_proposal);

    let receive_proposal_info = {
        'proposals':proposal_vote,
        'state': EXECUTE_STATE_PROCESSING,
        'vote_count': proposal_vote.length
    };

    let receive_relay_info = {
        'init_seq': receive_relay_db.init_seq,
        'complete_seq': receive_relay_db.complete_seq+1,
        'notary_list': receive_relay_db.notary_list,
        'chain_id': receive_relay_db.chain_id
    };

    if (parseInt(receive_proposal_info.vote_count) < parseInt(receive_relay_db.notary_list.length * vote_rate + 0.5)) {
        let tx = {
            'operations': [{
                'type': 4,
                'set_metadata': {
                    'key': receive_proposal + input.seq,
                    'value': JSON.stringify(receive_proposal_info)
                }
            }]
        };
        let isflag = callBackDoOperation(tx);
        if (!isflag) {
            throw 'Contract failure';
        }
        return;
    }

    receive_proposal_info.state = EXECUTE_STATE_SUCCESS;
    doIssue(input.t_assets_addr,thisAddress,input.to,input.seq,input.amount);
    let transaction = {
        'operations': [{
            'type': 4,
            'set_metadata': {
                'key': receive_proposal + input.seq,
                'value': JSON.stringify(receive_proposal_info)
            }
        },
        {
            'type': 4,
            'set_metadata': {
                'key': receive_relay,
                'value': JSON.stringify(receive_relay_info)
            }
        }]
    };
    let isSuccess = callBackDoOperation(transaction);
    if (!isSuccess) {
        throw 'Contract failure';
    }
}


function onReceiveProposalEvent(input){
    let relay = callBackGetAccountMetaData(thisAddress, receive_relay);
    if (relay === false) {
        throw 'Relay information does not exist';
    }

    let origin_proposal =  callBackGetAccountMetaData(thisAddress, receive_proposal+input.seq);

    let receive_relay_db = JSON.parse(relay.value);
    let complete_seq = receive_relay_db.complete_seq + 1;

    if (findListIndex(receive_relay_db.notary_list, sender) === false) {
        throw 'The operator is not a notary public';
    }

    if( input.t_chain_id!==receive_relay_db.chain_id||input.t_comm_addr!==thisAddress){
        throw 'args error';
    }

    if (input.seq !== complete_seq) {
        throw 'Inconsistent timing of communication information processing';
    }

    let proposal = {
        'seq': input.seq,
        'amount': input.amount,
        'from': input.from,
        'to': input.to,
        'f_assets_addr': input.f_assets_addr,
        't_assets_addr': input.t_assets_addr,
        'f_comm_addr': input.f_comm_addr,
        't_comm_addr': input.t_comm_addr,
        'f_chain_id': input.f_chain_id,
        't_chain_id': input.t_chain_id
    };

    if(origin_proposal===false){
        initReceiveProposalEvent(receive_relay_db,proposal,input);
        return ;
    }

    let receive_proposal_db = JSON.parse(origin_proposal.value);
    if(checkRecceiveNotary(receive_relay_db,receive_proposal_db,proposal,input)!==false){
        return;
    }

    handleReceiveElect(receive_relay_db,receive_proposal_db,proposal,input);
}


function main(inputData) {
    callBackLog('inputData:' + inputData);
    let input;
    try {
        input = JSON.parse(inputData);
    } catch(error) {
        return;
    }

    let action = input.
    function;
    switch (action) {
    case 'initRelayContract':
        initRelayContract(input);
        break;
    case 'sendCrossChain':
        sendCrossChain(input);
        break;
    case 'onSendProposalEvent':
        onSendProposalEvent(input);
        break;
    case 'onReceiveProposalEvent':
        onReceiveProposalEvent(input);
            break;
    default:
        throw 'Invalid operation type';
    }
}