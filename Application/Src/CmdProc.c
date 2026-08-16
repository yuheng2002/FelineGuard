#include <stdint.h>
#include <string.h>
#include "CmdProc.h"
#include "Comms.h"
#include "Feed.h"

void CmdProc_process(void){
	const char *command = Comms_PollCommand();

	if (!command) return;

	/* strcmp() takes in two char pointers
	 * returns 0 if strings are identical */
	if (strcmp(command, "FEED") == 0){
		if (!Feed_Request(FEED_CMD)){
			Comms_SendResponse("Busy feeding");
		}else{
			Comms_SendResponse("Feeding started");
		}
	}
	else if (strcmp(command, "PING") == 0){
		Comms_SendResponse("System ready");
	}
	else if (strcmp(command, "TIME") == 0){

	}
	else if (strcmp(command, "SCHED") == 0){

	}
#ifdef DEBUG
	else if (strcmp(command, "CRASH") == 0){
		/* hangs CPU deliberately */
		while (1){}
	}
	else if (strcmp(command, "CRASHFEED") == 0){

	}
#endif
	else{
		Comms_SendResponse("Invalid command");
	}
}
