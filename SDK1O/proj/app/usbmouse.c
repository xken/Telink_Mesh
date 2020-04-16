/********************************************************************************************************
 * @file     usbmouse.c 
 *
 * @brief    for TLSR chips
 *
 * @author	 telink
 *
 * @par      Copyright (c) Telink Semiconductor (Shanghai) Co., Ltd.
 *           All rights reserved.
 *           
 *			 The information contained herein is confidential and proprietary property of Telink 
 * 		     Semiconductor (Shanghai) Co., Ltd. and is available under the terms 
 *			 of Commercial License Agreement between Telink Semiconductor (Shanghai) 
 *			 Co., Ltd. and the licensee in separate contract or the terms described here-in. 
 *           This heading MUST NOT be removed from this file.
 *
 * 			 Licensees are granted free, non-transferable use of the information in this 
 *			 file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided. 
 *           
 *******************************************************************************************************/

#include "../tl_common.h"
#if(USB_MOUSE_ENABLE)

#include "usbmouse.h"
#include "../drivers/usb.h"
#include "../drivers/usbhw.h"
#include "../drivers/usbhw_i.h"
#include "../../vendor/common/rf_frame.h"

#ifndef	USB_MOUSE_REPORT_SMOOTH
#define	USB_MOUSE_REPORT_SMOOTH	0
#endif


#define  USBMOUSE_BUFF_DATA_NUM    8
static mouse_data_t mouse_dat_buff[USBMOUSE_BUFF_DATA_NUM];

static u8  usbmouse_wptr, usbmouse_rptr;
static u32 usbmouse_not_released;
static u32 usbmouse_data_report_time;



int Sum[2], Pre[2];
unsigned char smooth_cpi(signed char val, int xy){
	Sum[xy] = Sum[xy] - Pre[xy] + val;
	Pre[xy] = Sum[xy]/2;
	val = Pre[xy];
	return val;
}


void usbmouse_add_frame (rf_packet_mouse_t *packet_mouse){

	u8 *mdata_ptr;

	u8 new_data_num = packet_mouse->pno;  //����pno ����������ݵĸ���
	for(u8 i=0;i<new_data_num;i++)
	{

#if(1)  //sensor data smooth
			mdata_ptr = (u8 *)(&packet_mouse->data[i*sizeof(mouse_data_t)]);
			*(mdata_ptr+1) = smooth_cpi(*(mdata_ptr+1), 0);  //x smooth
			*(mdata_ptr+2) = smooth_cpi(*(mdata_ptr+2), 1);  //y smooth
			memcpy4((int*)(&mouse_dat_buff[usbmouse_wptr]), (int*)mdata_ptr, sizeof(mouse_data_t));
#else
			memcpy4((int*)(&mouse_dat_buff[usbmouse_wptr]), (int*)(&packet_mouse->data[i*sizeof(mouse_data_t)]), sizeof(mouse_data_t));
#endif
			BOUND_INC_POW2(usbmouse_wptr,USBMOUSE_BUFF_DATA_NUM);
			if(usbmouse_wptr == usbmouse_rptr)
			{
					//BOUND_INC_POW2(usbmouse_rptr,USBMOUSE_BUFF_DATA_NUM);
					break;
			}
	}
}


void usbmouse_release_check(){
	if(usbmouse_not_released && clock_time_exceed(usbmouse_data_report_time, USB_MOUSE_RELEASE_TIMEOUT)){
	    u32 release_data = 0;

	    if(usbmouse_hid_report(USB_HID_MOUSE, (u8*)(&release_data), MOUSE_REPORT_DATA_LEN)){
		    usbmouse_not_released = 0;
	    }
	}
}


void usbmouse_report_frame(){

#if 	USB_MOUSE_REPORT_SMOOTH
	static u32 tick = 0;
	if(usbhw_is_ep_busy(USB_EDP_MOUSE)) {
			tick = clock_time ();
	}

	u8 diff = (usbmouse_wptr - usbmouse_rptr) & (USBMOUSE_BUFF_DATA_NUM - 1);
	if (diff < 3 && !clock_time_exceed (tick, 5000)) {
		return;
	}
#endif

	if(usbmouse_wptr != usbmouse_rptr){
        u32 data = *(u32*)(&mouse_dat_buff[usbmouse_rptr]);	// that is   >  0
        int ret = usbmouse_hid_report(USB_HID_MOUSE,(u8*)(&data), MOUSE_REPORT_DATA_LEN);
		if(ret){
            BOUND_INC_POW2(usbmouse_rptr,USBMOUSE_BUFF_DATA_NUM);
		}
		if(0 == data && ret){			//  successfully  release the key
			usbmouse_not_released = 0;
		}else{
			usbmouse_not_released = 1;
			usbmouse_data_report_time = clock_time();
		}
	}
	return;
}


int usbmouse_hid_report(u8 report_id, u8 *data, int cnt){
	//unsigned char crc_in[8];
	//unsigned short crc;
	//unsigned int crch;

    assert(cnt<8);

    if (!reg_usb_host_conn)
    {
    	reg_usb_ep_ctrl(USB_EDP_MOUSE) = 0;
    	return 0;
    }

	if(usbhw_is_ep_busy(USB_EDP_MOUSE))
		return 0;

	reg_usb_ep_ptr(USB_EDP_MOUSE) = 0;

	// please refer to usbmouse_i.h mouse_report_desc
	extern u8 usb_mouse_report_proto;

	if (!usb_mouse_report_proto) {
		reg_usb_ep_dat(USB_EDP_MOUSE) = data[0];
		reg_usb_ep_dat(USB_EDP_MOUSE) = data[1];
		reg_usb_ep_dat(USB_EDP_MOUSE) = data[2];
	}
	else {
		reg_usb_ep_dat(USB_EDP_MOUSE) = report_id;
		foreach(i, cnt){
			reg_usb_ep_dat(USB_EDP_MOUSE) = data[i];
		}
	}
	reg_usb_ep_ctrl(USB_EDP_MOUSE) = FLD_EP_DAT_ACK;		// ACK

	return 1;
}


void usbmouse_init(){
}

#endif