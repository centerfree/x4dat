#include "task_radar.h"
#include "task_radar.h"
#include "clib/spProcessing.h"
//#include <stdlib.h>	// malloc
#include <fcntl.h>      // open()
#include <sys/ioctl.h>	// ioctl()
#include <sys/time.h>	// gettimeofday()
#include <time.h>       // localtime()
#include <math.h>       // fabs
#include <stdlib.h>     // system()
#include <unistd.h>     // sleep()

#define NOTI "1024_1432"
//#define X4_ENABLE	0	// IF board gpio7 - see artik530 sw user guide
//#define XTIO_X4_IO1	25	// IF board gpio6 - see artik530 sw user guide
//#define XTIO_X4_IO2	41	// IF board gpio5 - see artik530 sw user guide
//#define XTIO_X4_LDO     26      // LC board artik ldo enable pin
#define SPI_NAME	"/dev/uwbx4"
#define INPUT 1
#define OUTPUT 0
#define UNUSED(x) (void)(x)

RadarTaskParameters_t memtpram;
X4Driver_t memx4d;
XepDispatch_t memdis;
XepRadarX4DriverUserReference_t memusref;
radar_handle_t memrdhal;

uint8_t memspibuf[SPIBUFSIZE];
float32_t memfbin[FRAMESIZE];
float32_t preframe[FRAMESIZE];     // for replace frame loss or abnormal frame
static unsigned int datbuf[FRAMESIZE];  // for check raw data

uint32_t prefcnt;
uint32_t downbase;
uint32_t upbase;

float procdata[NUM_SAMPLES/5+3];

extern int _x4driver_unpack_and_normalize_frame(X4Driver_t* x4driver,float* bins_data, uint32_t bins_data_size ,uint8_t * raw_data, uint32_t raw_data_length);

static uint32_t read_and_send_radar_frame(X4Driver_t* x4driver, XepDispatch_t* dispatch);

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static uint32_t x4driver_callback_take_sem(void * sem, uint32_t timeout)
{
    UNUSED(sem);
    UNUSED(timeout);
    return 1;    // XEP_LOCK_OK
    //return xSemaphoreTakeRecursive((SemaphoreHandle_t)sem, timeout);
}

static void x4driver_callback_give_sem(void * sem)
{
    UNUSED(sem);
    //xSemaphoreGiveRecursive((SemaphoreHandle_t)sem);
}

static uint32_t x4driver_callback_pin_set_enable(void* user_reference, uint8_t value)
{
    radar_handle_t* pradar = ((XepRadarX4DriverUserReference_t*)user_reference)->radar_handle;
    int status = 0;

    (pradar->uwbioctl).value = value;
    status = ioctl(pradar->spi_dev, IOCTL_UWBX4_ENCTRL, &(pradar->uwbioctl));

    return status;
}

static uint32_t x4driver_callback_spi_write(void* user_reference, uint8_t* data, uint32_t length)
{
    // only 1byte write
    radar_handle_t* pradar = ((XepRadarX4DriverUserReference_t*)user_reference)->radar_handle;
    uwbx4ioctl_t* x4t = &(pradar->uwbioctl);

    if (0 == length)
    {
        return XEP_ERROR_X4DRIVER_OK;
    }
    if (NULL == data)
    {
        return XEP_ERROR_X4DRIVER_NOK;
    }

    x4t->addr = data[0];
    x4t->data_len = (length-1);
    x4t->databuf = &(data[1]);

    ioctl(pradar->spi_dev, IOCTL_UWBX4_SPIWR, x4t);

    return XEP_ERROR_X4DRIVER_OK;
}

static uint32_t x4driver_callback_spi_write_read(void* user_reference, uint8_t* wdata, uint32_t wlength, uint8_t* rdata, uint32_t rlength)
{
    radar_handle_t* pradar = ((XepRadarX4DriverUserReference_t*)user_reference)->radar_handle;
    uwbx4ioctl_t* x4t = &(pradar->uwbioctl);

    if ((0 == wlength) && (0 == rlength))
    {
        return XEP_ERROR_X4DRIVER_OK;
    }
    if ((NULL == wdata) || (NULL == rdata))
    {
        return XEP_ERROR_X4DRIVER_NOK;
    }

    if(wlength != 1)
    {
        printf("REG addr is wrong\n");
        return XEP_ERROR_X4DRIVER_NOK;
    }

    x4t->addr = wdata[0];
    x4t->data_len = rlength;
    x4t->databuf = rdata;

    ioctl(pradar->spi_dev, IOCTL_UWBX4_SPIRD, x4t);

    return XEP_ERROR_X4DRIVER_OK;
}

/************************************************************************/
/*   Simple dummy                                                       */
/************************************************************************/
void x4driver_callback_wait_us(uint32_t us)
{
    usleep(us);
}

uint32_t x4driver_timer_set_timer_timeout_frequency (void* timer, uint32_t frequency)
{
    UNUSED(timer);
    UNUSED(frequency);
    printf("task_radar.c : empty function - set_timer_timeout_frequency\n");
    // not implemented
    // used at timer mode
    return XEP_ERROR_X4DRIVER_OK;
}

void x4driver_notify_data_ready (void* user_reference)
{
    UNUSED(user_reference);
    printf("task_radar.c : empty function - notify_data_ready\n");
    // not implemented
    // used at timer mode
}

uint32_t x4driver_trigger_sweep_pin(void* user_reference)
{
    UNUSED(user_reference);
    printf("task_radar.c : empty function - trigger_sweep_pin\n");
    // not implemented
    // used at timer mode
    return XEP_ERROR_X4DRIVER_OK;
}

void x4driver_enable_ISR(void* user_reference, uint32_t enable)
{
    UNUSED(user_reference);
    DPRINTF("enable ISR %d\n", enable);
}

int a530_spiinitialize(void )
{
    int spifd;

    char *spi_dev = SPI_NAME;

    spifd = open(spi_dev, O_RDWR);
    if(spifd < 0)
    {
        printf("Error opening /dev/spidev2.0\n");
        return -1;
    }

    printf("uwbx4 driver open complete :: fd[%d]\n", spifd);

    return spifd;
}

int radar_hal_init(radar_handle_t ** radar_handle, void* instance_memory)
{
    int status = XEP_ERROR_X4DRIVER_OK;
    radar_handle_t * radar_handle_local = (radar_handle_t *)instance_memory;
    memset(radar_handle_local, 0, sizeof(radar_handle_t));
    radar_handle_local->radar_id = 0;

    radar_handle_local->spi_dev = a530_spiinitialize();
    if(radar_handle_local->spi_dev < 0){ return XEP_ERROR_X4DRIVER_NOK; }

    *radar_handle = radar_handle_local;

    return status;
}

uint32_t task_radar_init(X4Driver_t** x4driver, XepDispatch_t* dispatch)
{
    XepRadarX4DriverUserReference_t* x4driver_user_reference = &memusref;
    memset(x4driver_user_reference, 0, sizeof(XepRadarX4DriverUserReference_t));

    printf("uwb: @ x4driver ver[%s] start @\n", NOTI);

    int status = radar_hal_init((radar_handle_t **)(&(x4driver_user_reference->radar_handle)), (void *)(&memrdhal));

    //! [X4Driver Platform Dependencies]

// X4Driver lock mechanism, including methods for locking and unlocking.
    X4DriverLock_t lock;
    lock.object = NULL;   //(void*)xSemaphoreCreateRecursiveMutex();
    lock.lock = x4driver_callback_take_sem;
    lock.unlock = x4driver_callback_give_sem;

    // X4Driver lock mechanism, including methods for locking and unlocking.
    //uint32_t timer_id_sweep = 2;
    X4DriverTimer_t timer_sweep;
    timer_sweep.object = NULL; //xTimerCreate("X4Driver_sweep_timer", 1000 / portTICK_PERIOD_MS, pdTRUE, (void*)timer_id_sweep, x4driver_timer_sweep_timeout);
    timer_sweep.configure = x4driver_timer_set_timer_timeout_frequency;

    // X4Driver timer used for driver action timeout.
    //uint32_t timer_id_action = 3;
    X4DriverTimer_t timer_action;
    timer_action.object = NULL; //xTimerCreate("X4Driver_action_timer", 1000 / portTICK_PERIOD_MS, pdTRUE, (void*)timer_id_action, x4driver_timer_action_timeout);
    timer_action.configure = x4driver_timer_set_timer_timeout_frequency;

    // X4Driver callback methods.
    X4DriverCallbacks_t x4driver_callbacks;
    x4driver_callbacks.pin_set_enable = x4driver_callback_pin_set_enable;   // X4 ENABLE pin
    x4driver_callbacks.spi_read = x4driver_callback_spi_write;              // SPI read method - not implemented
    x4driver_callbacks.spi_write = x4driver_callback_spi_write;             // SPI write method
    x4driver_callbacks.spi_write_read = x4driver_callback_spi_write_read;   // SPI write and read method
    x4driver_callbacks.wait_us = x4driver_callback_wait_us;                 // Delay method
    x4driver_callbacks.notify_data_ready = x4driver_notify_data_ready;      // Notification when radar data is ready to read
    x4driver_callbacks.trigger_sweep = x4driver_trigger_sweep_pin;          // Method to set X4 sweep trigger pin
    x4driver_callbacks.enable_data_ready_isr = x4driver_enable_ISR;         // Control data ready notification ISR

//! [X4Driver Platform Dependencies]

    x4driver_create(x4driver, (void*)(&memx4d), &x4driver_callbacks,&lock,&timer_sweep,&timer_action, (void*)x4driver_user_reference);

    RadarTaskParameters_t* task_parameters = &memtpram;
    task_parameters->dispatch = dispatch;
    task_parameters->x4driver = *x4driver;

    task_parameters->dispatch->p_fbin = &(memfbin[0]);
    task_parameters->x4driver->spi_buffer_size = SPIBUFSIZE;
    task_parameters->x4driver->spi_buffer = &(memspibuf[0]);
    if ((((uint32_t)task_parameters->x4driver->spi_buffer) % 32) != 0)
    {
        int alignment_diff = 32 - (((uint32_t)task_parameters->x4driver->spi_buffer) % 32);
        task_parameters->x4driver->spi_buffer += alignment_diff;
        task_parameters->x4driver->spi_buffer_size -= alignment_diff;
    }
    task_parameters->x4driver->spi_buffer_size -= task_parameters->x4driver->spi_buffer_size % 32;

    status = (task_radar((void *)task_parameters));

    return status;
}

void user_app(X4Driver_t* x4driver)
{
    uint16_t dac_min =0;
    uint16_t dac_max =0;
    uint8_t iterations = 0x00;
    uint16_t pulses_per_step = 0x0000;
    float32_t fa_ofset = 0.0;
    float32_t fa_st = 0.0;
    float32_t fa_end = 0.0;
    uint8_t prf_d = 0x00;
    float32_t fps = 0;
    xtx4_dac_step_t dac_step = 0x00;
    uint32_t baseline = 0;

#ifdef D10M
    // Set DAC range
    x4driver_set_dac_max(x4driver,1100);            // 10M
    //x4driver_set_dac_max(x4driver,1150);            // sleep_test_0817
    x4driver_set_dac_min(x4driver,949);

    // Set Integration
    x4driver_set_iterations(x4driver,16);           // 10M
    x4driver_set_pulses_per_step(x4driver,32);      // 10M
    //x4driver_set_pulses_per_step(x4driver,32);      // sleep_test_0817
    //x4driver_set_iterations(x4driver,32);           // sleep_test_0817

    // Set Frame Area
    x4driver_set_frame_area_offset(x4driver, 0.18);
    // x4driver_set_frame_area 10M - none

    // Set PRF Divide
    x4driver_set_prf_div(x4driver, 16);   // 10M
    //x4driver_set_prf_div(x4driver, 6);    // sleep_test_0817

#else    // D6M
    // Set DAC range
    x4driver_set_dac_max(x4driver,1150);            // 6M
    x4driver_set_dac_min(x4driver,949);

    // Set Integration
    x4driver_set_iterations(x4driver,32);           // 6M
    x4driver_set_pulses_per_step(x4driver,1);       // 6M

    // Set Frame Area
    x4driver_set_frame_area_offset(x4driver, 0.18);
    x4driver_set_frame_area(x4driver, 0.15, 6.11);  // 6M

    // Set PRF Divide
    x4driver_set_prf_div(x4driver, 11);    // 6M
#endif

    x4driver_set_fps(x4driver,FPS);

    x4driver_get_dac_min(x4driver,&dac_min);
    x4driver_get_dac_max(x4driver,&dac_max);
    x4driver_get_iterations(x4driver,&iterations);
    x4driver_get_pulses_per_step(x4driver,&pulses_per_step);
    x4driver_get_frame_area_offset(x4driver, &fa_ofset);
    x4driver_get_frame_area(x4driver, &fa_st, &fa_end);
    x4driver_get_dac_step(x4driver, &dac_step);

    x4driver_get_prf_div(x4driver, &prf_d);
    x4driver_get_fps(x4driver,&fps);

    baseline = (uint32_t)(((float32_t)((dac_max - dac_min + 1)*iterations*pulses_per_step)*1024.0)/(2048.0*(float32_t)(1<<dac_step)));
    DPRINTF("t_ uwb: @ dac max[%d] , min[%d], dacstep[%d], iter[%d], step[%d], f_offset[%f], f_st[%f], f_end[%f], prf_div[%d], fps[%f] @\n"
    	,dac_max, dac_min, dac_step, iterations, pulses_per_step, fa_ofset, fa_st, fa_end, prf_d, fps);	// _tesst

    downbase = baseline*0.98;
    upbase = baseline*1.02;
    DPRINTF("uwb: @ base[%d] - %d ~ %d @\n", baseline, downbase, upbase);

    uint8_t trx_back_end = 0x00;
    x4driver_get_pif_register(x4driver,ADDR_PIF_MCLK_TRX_BACKEND_CLK_CTRL_RW,&trx_back_end);

    uint8_t rx_pll_ctrl = 0x00;
    x4driver_get_pif_register(x4driver, ADDR_PIF_RX_PLL_CTRL_1_RW, &rx_pll_ctrl);

    uint8_t common_1 = 0x00;
    x4driver_get_pif_register(x4driver, ADDR_PIF_COMMON_PLL_CTRL_1_RW, &common_1);

    DPRINTF("uwb: @ trx_back[%02X] , rx_pll_1[%02X] , common_1[%02X] @\n"
        	,trx_back_end, rx_pll_ctrl, common_1);	// _tesst2

    DPRINTF("%s%d second recordings...\n", TAG, RECORDSEC);	// debug
}

int moveFileForUpload(char *svfname)
{
    int ret = 0;


#ifdef DATSAVE
    char cmd[256] = {0, };
    sprintf(cmd, "mv %s%s.x4dat /sdcard/fall_record/uwb/%s.x4dat &", SAVPATH, svfname, svfname);
    ret = system(cmd);
#endif  // DATSAVE


// ashish
// call python code to analyse this file
#ifdef PROGRAM
    char pgm[256] = {0, };
    sprintf(pgm, "%s /sdcard/fall_record/uwb/%s.x4dat &",PROGRAM, svfname);
    ret = ret * system(pgm);
#endif  // PROGRAM


#ifdef PREPROC
    char cmd[256] = {0, };

    sprintf(cmd, "mv %s%s.bbdat /sdcard/fall_record/uwb/%s.bbdat &", SAVPATH, svfname, svfname);
    ret = system(cmd);
#endif  // PREPROC

    return ret;
}

int deleteFile(char* svfname)
{
    int ret = 0;

#ifdef DATSAVE
    char cmd[256] = { 0, };

    sprintf(cmd, "rm %s%s.x4dat &", SAVPATH, svfname);
    ret = system(cmd);
#endif  // DATSAVE

#ifdef PREPROC
    char cmd[256] = { 0, };

    sprintf(cmd, "rm %s%s.bbdat &", SAVPATH, svfname);
    ret = system(cmd);
#endif  // PREPROC

    return ret;
}

int saveframe(X4Driver_t* x4driver, XepDispatch_t* dispatch, char *recfname)
{
    int ret = 0;
    int ffd = 0;
    char fName[256];

    uint32_t frame_len = dispatch->fbin_cnt;    // 1536 -> 929

    // test cho find abnormal frame and frame loss
    unsigned char lossflag = 0;

    // loss check
    if((prefcnt+1) != x4driver->frame_counter){
        printf("uwb: @ Frame loss : %d -> %d @\n", prefcnt, x4driver->frame_counter);
        lossflag = 1;
    }
    else
    {
        lossflag = 0;
    }

#ifdef FRAMECHECK
    int i = 0;
    unsigned int sum = 0;
    unsigned char abnormflag = 0;
    float32_t* p_save = 0;

    if(prefcnt < 20)
    {
        abnormflag = 0;
        lossflag = 0;
        p_save = dispatch->p_fbin;
    }
    else{
        // abnormal check
        for(i=288; i<392; i++){ sum += (x4driver->rawdatbuf)[i]; }
        sum = sum/104;    // 752-648
        if((sum<downbase)||(sum>upbase))
        {
            printf("uwb: @ abnFrame fcnt : %d @\n", x4driver->frame_counter);
            abnormflag = 1;

            // replace to preframe, only x4dat
            p_save = preframe;
        }
        else
        {
            abnormflag = 0;
            p_save = dispatch->p_fbin;
            //copy buf to preframe
            if(memmove(preframe, dispatch->p_fbin, (frame_len+3)*4) == 0)
            {
                printf("uwbx4 error at preframe copy\n");
                ret = 1;
            }
        }
    }

    /* static unsigned char preioflag = 0;
    radar_handle_t* tmpradar = ((XepRadarX4DriverUserReference_t*)x4driver->user_reference)->radar_handle;
    if(abnormflag||lossflag){
        if(preioflag == 0){
            (tmpradar->uwbioctl).value = 1;
            ret = ioctl(tmpradar->spi_dev, IOCTL_UWBX4_IO2CTRL, &(tmpradar->uwbioctl));
        }
    }
    else{
        if(preioflag == 1){
            (tmpradar->uwbioctl).value = 0;
            ret = ioctl(tmpradar->spi_dev, IOCTL_UWBX4_IO2CTRL, &(tmpradar->uwbioctl));
        }
    }
    preioflag = abnormflag||lossflag;*/
    /////////////////////////////////////////

#ifdef DATSAVE
    sprintf(fName, "%s%s.x4dat", SAVPATH, recfname);

    ffd = open(fName, O_RDWR | O_APPEND | O_CREAT, 0644);
    if(ffd < 0)
    {
        printf("uwb data file open error\n");
        ret = -1;
    }
    if(lossflag)
    {
        for(i=1; (prefcnt+i)!=(x4driver->frame_counter); i++)
        {
            ((uint32_t *)preframe)[0] = 0xCC3377AA;       // header, ID, LSB first send
            ((uint32_t *)preframe)[1] = prefcnt+i;  // Frame counter
            ((uint32_t *)preframe)[2] = frame_len;  // bin length, maybe 929
            if(write(ffd, (char *)preframe, (frame_len+3)*4) < 0)    // 929bins + 3header = 932 * 4byte
            {
                printf("uwb preframe file write error\n");
                ret = -1;
            }
        }
    }
    ((uint32_t *)p_save)[0] = 0xCC3377AA;       // header, ID, LSB first send
    ((uint32_t *)p_save)[1] = x4driver->frame_counter;  // Frame counter
    ((uint32_t *)p_save)[2] = frame_len;  // bin length, maybe 929
    if(write(ffd, (char *)p_save, (frame_len+3)*4) < 0)    // 929bins + 3header = 932 * 4byte
    {
        printf("uwb data file write error\n");
        ret = -1;
    }
    close(ffd);
#endif  // DATSAVE

#else    // FRAMECHECK
    UNUSED(lossflag);
#ifdef DATSAVE
    sprintf(fName, "%s%s.x4dat", SAVPATH, recfname);

    ffd = open(fName, O_RDWR | O_APPEND | O_CREAT, 0644);
    if(ffd < 0)
    {
        printf("uwb data file open error\n");
        ret = -1;
    }
    ((uint32_t *)dispatch->p_fbin)[0] = 0xCC3377AA;       // header, ID, LSB first send
    ((uint32_t *)dispatch->p_fbin)[1] = x4driver->frame_counter;  // Frame counter
    ((uint32_t *)dispatch->p_fbin)[2] = frame_len;  // bin length, maybe 929
    if(write(ffd, (char *)(dispatch->p_fbin), (frame_len+3)*4) < 0)    // 929bins + 3header = 932 * 4byte
    {
        printf("x4dat file write error\n");
        ret = -1;
    }
    close(ffd);
#endif  // DATSAVE
#endif  // FRAMECHECK

#ifdef SPISAVE
    int ffd222 = 0;
    char fName222[256];
    sprintf(fName222, "%s%s.spidat", SAVPATH, recfname);

    ffd222 = open(fName222, O_RDWR | O_APPEND | O_CREAT, 0644);
    if(ffd222 < 0)
    {
        printf("rawdata file open error\n");
        ret = -1;
    }
    // csj test 0712 1656, to check raw data
    (x4driver->rawdatbuf)[0] = 0xCC3377AA;       // header, ID, LSB first send
    (x4driver->rawdatbuf)[1] = x4driver->frame_counter;  // Frame counter
    (x4driver->rawdatbuf)[2] = frame_len;  // bin length, maybe 929
    if(write(ffd222, (char *)(x4driver->rawdatbuf), (frame_len+3)*4) < 0)    // 929bins + 3header = 932 * 4byte
    {
        printf("rawdata file write error\n");
        ret = -1;
    }
    close(ffd222);
#endif

#ifdef PREPROC
    int ffd3 = 0;
    char fName3[256];
    // csj test 0712 1656, to check raw data
    if((frame_len - NUM_SAMPLES) > 0)
    {
        ecsol_Proc((float *)&(dispatch->p_fbin[frame_len-NUM_SAMPLES]), &procdata[3]);   // rawdata,  dismap, doppmap
    }
    else
    {
        printf("baseband data length is too small\n");
        ret = -1;
    }

    sprintf(fName3, "%s%s.bbdat", SAVPATH, recfname);

    ffd3 = open(fName3, O_RDWR | O_APPEND | O_CREAT, 0644);
    if(ffd3 < 0)
    {
        printf("baseband data file open error\n");
        ret = -1;
    }

    ((unsigned int*)procdata)[0] = 0xCC3377AA;       // header, ID, LSB first send
    ((unsigned int*)procdata)[1] = x4driver->frame_counter;  // Frame counter
    ((unsigned int*)procdata)[2] = NUM_SAMPLES/5;  // bin length, maybe 929
    if(write(ffd3, (char *)procdata, (NUM_SAMPLES/5+3)*4) < 0)    // 929bins + 3header = 932 * 4byte
    {
        printf("baseband file write error\n");
        ret = -1;
   }
    close(ffd3);
#endif

    prefcnt = x4driver->frame_counter;
    return ret;
}

int task_radar(void* pvParameters)
{
        RadarTaskParameters_t* task_parameters = (RadarTaskParameters_t*)pvParameters;
        XepDispatch_t* dispatch = (XepDispatch_t*)(task_parameters->dispatch);
        X4Driver_t* x4driver = (X4Driver_t*)(task_parameters->x4driver);
        radar_handle_t* pradar = ((XepRadarX4DriverUserReference_t*)x4driver->user_reference)->radar_handle;

        int status = 0;
        status = x4driver_set_enable(x4driver, 1);
        usleep(100);

        xtx4driver_errors_t tmp_status = x4driver_init(x4driver);

        status = x4driver_check_configuration(x4driver);
        x4driver_set_sweep_trigger_control(x4driver, SWEEP_TRIGGER_X4); // By default let sweep trigger control done by X4

        if (tmp_status != XEP_ERROR_X4DRIVER_OK) {
            printf("uwb: x4 init set fail (ERR code : %d)\r\n", tmp_status);
            spi_wrtest(x4driver, 2);
            return 1;
        }
        else if (status != XEP_ERROR_X4DRIVER_OK) {
            printf("uwb: x4 init verify fail (ERR code : %d)\r\n", status);
            spi_wrtest(x4driver, 2);
            return 1;
        }
        else {
            printf("uwb: x4 init finish (::%d::%d)\r\n", tmp_status, status);
            //ioctl(pradar->spi_dev, IOCTL_UWBX4_SPITEST, &(pradar->uwbioctl));
            //ioctl(pradar->spi_dev, IOCTL_UWBX4_SPITEST, &(pradar->uwbioctl));
            //return 0;
        }

        user_app(x4driver);

        (pradar->uwbioctl).frame_read_size = x4driver->frame_read_size;
        (pradar->uwbioctl).start_flag = 1;
        ioctl(pradar->spi_dev, IOCTL_UWBX4_FRAMEONOFF, &(pradar->uwbioctl));
        DPRINTF("uwb: frame_read_size : %d\n", x4driver->frame_read_size);

        // ashish
        // if lib ver, does not enter the loop.
        if (dispatch->task_type == 1)    // 0 : app version,  1 : lib version
        {
            printf("%s@ lib version, still forcing through user app loop\n", TAG);
            // return 0;
        }
        else {
            printf("%s@ standalone application : rec %dsec @\n", TAG, RECORDSEC);
            //xep_close(x4driver, dispatch);
            //return 0;
        }

        // standalone run
        //struct timeval pre_time;
        //struct timeval post_time;
        struct timeval record_time;
        struct tm* ptm;
        int ret = 0;
        unsigned int stcnt = 0;
        char time_string[40] = { 0, };
        char recfname[50] = { 0, };
        float32_t fps = 0;
        x4driver_get_fps(x4driver, &fps);
        InitializeUWBSP(0);

        x4driver->rawdatbuf = datbuf;    // csj test 0712 1656, to check raw data
        ioctl(pradar->spi_dev, IOCTL_UWBX4_FRAMERESET, &(pradar->uwbioctl));
        UNUSED(read_and_send_radar_frame);

        // while(1)
        for (int m = 0; m < 99999; m++)
        {
            // If it's not the first time, move the previous file to the upload folder and start the next save.
            if (strlen(recfname) != 0) {
                if ((5 * m < SKIP) || ((m-1) % SKIP == 0)) moveFileForUpload(recfname);
                else deleteFile(recfname);
            }

            // make file name (timestamp)
            gettimeofday(&record_time, NULL);
            ptm = localtime(&record_time.tv_sec);
            strftime(time_string, sizeof(time_string), "%Y-%m-%d_%H-%M-%S", ptm);
            long milliseconds = record_time.tv_usec / 1000;
            sprintf(recfname, "%s.%03ld", time_string, milliseconds);
            printf("uwb: %s%s.x4dat\n", SAVPATH, recfname);
            printf("%srecording number %d\n", TAG, m);


            for (int n = 0; n < RECORDSEC; n++)
            {
                if (ret != 0) { break; }

                //gettimeofday(&pre_time, NULL);
                for (int k = 0; k < ((int)fps); )
                {
                    (pradar->uwbioctl).framebuf = x4driver->spi_buffer;
                    ioctl(pradar->spi_dev, IOCTL_UWBX4_FRAMEREAD, &(pradar->uwbioctl));
                    x4driver->frame_counter = (pradar->uwbioctl).frame_counter;

                    uint32_t fdata_count = 0;
                    x4driver_get_frame_bin_count(x4driver, &fdata_count);
                    dispatch->fbin_cnt = fdata_count;

                    if ((k == 0) && (n == 0)) { DPRINTF("uwb: spisize[%d] , framesize[%d]\n", x4driver->frame_read_size, dispatch->fbin_cnt); }  // temp test

                    _x4driver_unpack_and_normalize_frame(x4driver, &((dispatch->p_fbin)[3]), fdata_count, x4driver->spi_buffer, x4driver->frame_read_size);

                    // Check for failure and shut down transmitter
                    if (x4driver->zero_frame_counter >= X4DRIVER_MAX_ALLOWED_ZERO_FRAMES)
                    {
                        x4driver->zero_frame_counter = 0;
                        x4driver_set_tx_power(x4driver, TX_POWER_OFF);
                        x4driver_set_fps(x4driver, 0);
                    }

                    // make Packet and save
                    ret += saveframe(x4driver, dispatch, recfname);
                    if (ret == 0) {
                        if (k == 0) {
                            stcnt = x4driver->frame_counter;
                            k++;
                        }
                        else { k = 1 + x4driver->frame_counter - stcnt; }
                    }
                    else {
                        printf("uwb get frame error[%d]\n", ret);
                        k = fps;
                    }
                }
                //gettimeofday(&post_time, NULL);
                //printf("uwb: %d-%d : %ld us\n",stcnt,x4driver->frame_counter,1000000*(post_time.tv_sec-pre_time.tv_sec)+(post_time.tv_usec-pre_time.tv_usec));
            }
        }

        printf("uwb: avail : %d,  max : %d,  overflow : %d\n", (pradar->uwbioctl).availframe, (pradar->uwbioctl).maxframe, (pradar->uwbioctl).overflow);

        status = xep_close(x4driver, dispatch);

    return status;
}

static uint32_t read_and_send_radar_frame(X4Driver_t* x4driver, XepDispatch_t* dispatch)
{
    uint32_t status=0;

    uint32_t bin_count = 0;
    x4driver_get_frame_bin_count(x4driver,&bin_count);
    uint8_t down_conversion_enabled = 0;
    x4driver_get_downconversion(x4driver,&down_conversion_enabled);

    uint32_t fdata_count = bin_count;
    if(down_conversion_enabled == 1)
    {
        fdata_count = bin_count * 2;
    }
    dispatch -> fbin_cnt = fdata_count;

    // Read radar data into dispatch memory.
    uint32_t dummyfcounter = 0;
    status = x4driver_read_frame_normalized(x4driver, &dummyfcounter, &((dispatch->p_fbin)[2]), fdata_count);

    // Check for failure and shut down transmitter
    if (x4driver->zero_frame_counter >= X4DRIVER_MAX_ALLOWED_ZERO_FRAMES)
    {
        x4driver->zero_frame_counter = 0;
        x4driver_set_tx_power(x4driver, TX_POWER_OFF);
        x4driver_set_fps(x4driver, 0);
    }

    return status;
}

int xep_close(X4Driver_t* x4driver, XepDispatch_t* dispatch)
{
    XepRadarX4DriverUserReference_t* user_reference = ((X4Driver_t *)x4driver)->user_reference;
    radar_handle_t* x4driver_user_reference = user_reference->radar_handle;
    UNUSED(dispatch);

    close(x4driver_user_reference->spi_dev);
    x4driver->callbacks.enable_data_ready_isr(x4driver->user_reference,0);
    //close(dispatch->soc_handle);

    printf("close x4driver\n");

    return 0;
}

int xep_gpioinit(void )
{
    printf("task_radar.c : empty function - xep_gpioinit\n");

    return 0;
}

// to verify spi r/w low level function
int spi_wrtest(X4Driver_t* x4driver, int ncnt)
{
    uint8_t r_buf = 0;
    int status = 0;
    int i = 0;

    for(i=0; i<ncnt; i++)
    {
        for(uint8_t d = 0xFF; d>0x00; d--)
        {
            x4driver_set_spi_register(x4driver, ADDR_SPI_DEBUG_RW, d);
            x4driver_get_spi_register(x4driver, ADDR_SPI_DEBUG_RW, &r_buf);
            if(d != r_buf)
            {
                printf("x4 spi r/w fail[w%02X r%02X]\r\n", d, r_buf);
                status = 1;
            }
        }
    }

    return status;
}

int xep_init(int runtype)    // 0 : app version,  1 : lib version
{
    int status = -1;    // normal 0, exept 0 abnormal
    void *dispatch;
    void *x4driver;
    dispatch = (void *)&memdis;
    memset(dispatch, 0, sizeof(XepDispatch_t));
    ((XepDispatch_t*)dispatch)->task_type = runtype;
    status = task_radar_init((X4Driver_t**)(&x4driver), (XepDispatch_t*)dispatch);
    if(status != 0)
    {
        xep_close((X4Driver_t*)x4driver, (XepDispatch_t*)dispatch);
    }

    return status;
}

int get_struct(void* x4driver, void* dispatch)
{
    dispatch = (void *)&memdis;
    x4driver = (void *)&memx4d;

    UNUSED(dispatch);
    UNUSED(x4driver);

    return 0;
}

int xep_getframe(float32_t* framealloc)
{
    XepDispatch_t* dispatch = &memdis;
    X4Driver_t* x4driver = &memx4d;
    radar_handle_t* pradar = ((XepRadarX4DriverUserReference_t*)x4driver->user_reference)->radar_handle;
    uint32_t fdata_count = 0;
    int ret =0;
    static int testcnt = 0;
    int i = 0;
    unsigned int sum = 0;
    unsigned char abnormflag = 0;
    char abnorm_msg[32] = {0, };

    if(testcnt == 0)
    {
        InitializeUWBSP(0);

        x4driver->rawdatbuf = datbuf;    // csj test 0712 1656, to check raw data
        ioctl(pradar->spi_dev, IOCTL_UWBX4_FRAMERESET, &(pradar->uwbioctl));
    }

    (pradar->uwbioctl).framebuf = x4driver->spi_buffer;
    ioctl(pradar->spi_dev, IOCTL_UWBX4_FRAMEREAD, &(pradar->uwbioctl));
    x4driver->frame_counter = (pradar->uwbioctl).frame_counter;

    x4driver_get_frame_bin_count(x4driver, &fdata_count);
    dispatch -> fbin_cnt = fdata_count;

    ((uint32_t *)framealloc)[0] = 0xCC3377AA;       // header, ID, LSB first send
    ((uint32_t *)framealloc)[1] = x4driver->frame_counter;  // Frame counter
    ((uint32_t *)framealloc)[2] = dispatch->fbin_cnt;  // bin length, maybe 929

    ret = _x4driver_unpack_and_normalize_frame(x4driver,&(framealloc[3]),fdata_count,x4driver->spi_buffer,x4driver->frame_read_size);

    if(testcnt < 20)
    {
        if(testcnt == 0){ DPRINTF("uwb: spisize[%d] , framesize[%d]\n" , x4driver->frame_read_size, dispatch->fbin_cnt); }
        abnormflag = 0;
    }
    else{
        // abnormal check
        for(i=288; i<392; i++){ sum += (x4driver->rawdatbuf)[i]; }
        sum = sum/104;    // 752-648
        if((sum<downbase)||(sum>upbase))
        {
            sprintf(abnorm_msg, "abnFrame fcnt - %d", x4driver->frame_counter);      // log_msg should not have \n
            xep_save_log(abnorm_msg);

            abnormflag = 1;
        }
        else
        {
            abnormflag = 0;
        }
    }

    if(abnormflag == 1)
    {
        // if abnormal frame, copy preframe to appframe memory
        if(memmove(&(framealloc[3]), preframe, (fdata_count+3)*4) == 0)
        {
            printf("uwbx4 error at preframe copy\n");
            ret = 1;
        }
    }
    else
    {
        // if normal frame, save frame to preframe
        if(memmove(preframe, &(framealloc[3]), (fdata_count+3)*4) == 0)
        {
            printf("uwbx4 error at preframe copy\n");
            ret = 1;
        }
    }

    testcnt++;

    return ret;
}

int x4_Processing(float* pCounters, float* pOutputDist, float* pOutputDoppler)
{
    // arg : In float32 700, out float32 128, out float32 128
    Processing(pCounters, pOutputDist, pOutputDoppler);		//clib/spProcessing.c

    return 0;
}

int xep_save_log(char* log_msg)  // log_msg should not have \n
{
    struct timeval log_time;
    struct tm *log_ptm;
    char time_string[32] = {0, };
    char log_string[64] = {0, };
    int ffd = 0;
    char fname[64] = {0, };
    int ret = 0;

    gettimeofday(&log_time, NULL);
    log_ptm = localtime(&log_time.tv_sec);
    strftime(time_string, sizeof(time_string), "%Y%m%d_%H%M%S", log_ptm);
    sprintf(log_string, "%s : %s\n", time_string, log_msg);
    printf("%s", log_string);

    sprintf(fname, "%s/uwb_log.txt", SAVPATH);
    ffd = open(fname, O_RDWR | O_APPEND | O_CREAT, 0644);
    if(ffd < 0)
    {
        printf("uwb log file open error\n");
        ret = -1;
    }
    if(write(ffd, log_string, strlen(log_string)) < 0)
    {
        printf("uwb log file write error\n");
        ret = -1;
    }
    close(ffd);

    return ret;
}

/**********************************************/
