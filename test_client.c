#include "common.h"
#include "socketlib.h"




#undef  DBG_ON
#undef  FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"test_client:"



int main(void)
{

	int ret = -1;
	dbg_printf("this is a test for client!\n");
	ret = socketlib_init_handle();
	dbg_printf("ret==%d\n",ret);
	return(0);
}
