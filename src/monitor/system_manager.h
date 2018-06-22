
#ifndef SYSTEM_MANAGER_H_
#define SYSTEM_MANAGER_H_

#include <utils/system.h>
#include <common/general.h>
#include <proto/cpp/monitor.pb.h>

namespace bubi {
	class SystemManager {
	public:
		SystemManager();
		~SystemManager();

	public:
		void OnSlowTimer(int64_t current_time);

		bool GetSystemMonitor(std::string paths, monitor::SystemStatus* &system_status);
		bool GetCurrentMonitor(double &current_cpu_percent, double &current_memory_percent);

	private:
		utils::System system_;      // os
		double cpu_used_percent_;   // cpu percent
		int64_t check_interval_;    // timer interval
		int64_t last_check_time_;   // last check time

		double current_cpu_used_percent_; // current process cpu percent
	};
}

#endif
