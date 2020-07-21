/**
 * @file
 * @brief Platform independent driver to interface with x4 radar.
 * @for ARTIK530 and linux
 */

#ifndef TASK_RADAR_H
#define TASK_RADAR_H

#include <unistd.h>	// read(), write(), close, lseek, usleep
#include <string.h>	// memset
#include "x4driver.h"
#include "uwbx4.h"

//--- Comment out any of the function below if you do not want to. ---
#define DBGMSG          // Debug message
//#define FRAMECHECK      // Abnormal frame baseband check
#define DATSAVE         // x4dat save
//#define SPISAVE         // spidat svae
//#define PREPROC         // baseband processing save

#define D10M    // 10M set,  else 6M

#ifdef DBGMSG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...) do{}while(0)
#endif
//--------------------------------------------------------------------

#define  FRAMESIZE      1664                       // Expected max size
#define  SPIBUFSIZE     FRAMESIZE*3

#define  RECORDSEC      6                           // Recording duration 
#define  SAVPATH        "/sdcard/fall_record/tmp/"   //""
#define  FPS            100

#define  SKIP           3                           // only save every "SKIP" files
#define  TAG            "uwb: DEBUG: "

#define  PROGRAM        "python3 /home/ec-solution/uwb_presence/presence.py"

typedef struct
{
    void* radar_handle;				// radar_handle_t
} XepRadarX4DriverUserReference_t;


/****************************************************************************
 * typedef : xep_dispatch.h  ,   xtserial_definition.h   ,   xep_dispatch_messages.h
 ****************************************************************************/

#define XEP_ERROR_OK				0

typedef struct
{
//    uint32_t notify_value;
//    uint32_t message_id_counter;
//    MemoryPoolSet_t* memorypoolset;	//MemoryPoolSet
    int soc_handle;		// client socket handler to send data
    int task_type;		// 0 : app version,  1 : lib version
    uint32_t fbin_cnt;		// float buffer length
    float32_t* p_fbin;		// for float type binary (final data)
//    XepDispatchSubscriberInfo_t subscribers[XEP_DISPATCH_MESSAGETAG_COUNT];

}XepDispatch_t;	// not use dispatch, only declaration for function argument

/****************************************************************************
 * typedef : radar_hal.h  ,  task_radar.h   ,   TizenRT object
 ****************************************************************************/
typedef struct {
    int radar_id;                   ///< Id of current radar

    int spi_dev;  	// spi_dev file descriptor
    uwbx4ioctl_t uwbioctl;    // uwb ioctl control struct

}radar_handle_t;

typedef struct
{
    XepDispatch_t* dispatch;
    X4Driver_t* x4driver;
} RadarTaskParameters_t;

/****************************************************************************
 * function : task_radar.h
 ****************************************************************************/

/**
 * @brief Initialize Radar task.
 *
 * @return Status of execution
 */
uint32_t task_radar_init(X4Driver_t** x4driver, XepDispatch_t* dispatch);

/**
 * Initiate Radar HAL
 *
 * @param  radar_handle    Pointer to Radar handle is returned in this variable
 * @param  instance_memory Pointer to memory allocated before calling this function
 * @return                 Status of execution.
 */
int radar_hal_init(radar_handle_t ** radar_handle, void* instance_memory);

int task_radar(void* pvParameters);

/****************************************************************************
 * function : xep_dispatch.h  ,  xep_application.h   ,   xep_dispatch_messages.h
 ****************************************************************************/

int xep_close(X4Driver_t* x4driver, XepDispatch_t* dispatch);
int xep_gpioinit(void );
int spi_wrtest(X4Driver_t* x4driver, int ncnt);
int xep_init(int runtype);    // 0 : app version,  1 : lib version
int get_struct(void* x4driver, void* dispatch);
int xep_getframe(float32_t* framealloc);
int moveFileForUpload(char *svfname);
int x4_Processing(float* pCounters, float* pOutputDist, float* pOutputDoppler);
int xep_save_log(char* log_msg);  // log_msg should not have \n
/******************************/

#endif // TASK_RADAR_H
