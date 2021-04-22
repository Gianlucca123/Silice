/*

    Silice FPGA language and compiler
    (c) Sylvain Lefebvre - @sylefeb

This work and all associated files are under the

     GNU AFFERO GENERAL PUBLIC LICENSE
        Version 3, 19 November 2007
        
A copy of the license full text is included in 
the distribution, please refer to it for details.

(header_1_0)
*/
//
// Based on code from the MCC216 project (https://github.com/fredrequin/fpga_1943)
// (c) Frederic Requin, GPL v3 license
//

#include "verilated.h"
#include "video_out.h"
#include "LibSL/Image/Image.h"
#include "LibSL/Image/ImageFormat_TGA.h"
#include "LibSL/Math/Vertex.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

using namespace LibSL::Image;
using namespace LibSL::Math;

ImageFormat_TGA g_RegisterFormat_TGA;

// Constructor 
VideoOut::VideoOut(vluint8_t debug, vluint8_t depth, vluint8_t polarity,
                   vluint16_t hactive, vluint16_t hfporch_, vluint16_t hspulse_, vluint16_t hbporch_,
                   vluint16_t vactive, vluint16_t vfporch_, vluint16_t vspulse_, vluint16_t vbporch_,
                   const char *file)
{
    // color depth
    if (depth <= 8) {
        bit_mask  = (1 << depth) - 1;
        bit_shift = (int)(8 - depth);
    } else {
        bit_mask  = (vluint8_t)0xFF;
        bit_shift = (int)0;
    }
    // synchros polarities
    hs_pol      = (polarity & HS_POS_POL) ? (vluint8_t)1 : (vluint8_t)0;
    vs_pol      = (polarity & VS_POS_POL) ? (vluint8_t)1 : (vluint8_t)0;
    // screen format initialized
    hor_size    = hactive;
    ver_size    = vactive;
    // record synch values
    hfporch = hfporch_;
    hspulse = hspulse_;
    hbporch = hbporch_;
    vfporch = vfporch_;
    vspulse = vspulse_;
    vbporch = vbporch_;
    // debug mode
    dbg_on      = debug;
    // allocate the pixels
    printf("%d x %d x %d\n",hactive,vactive,(int)depth);
    pixels.allocate((int)hactive, (int)vactive);
    // copy the filename
    filename    = std::string(file);
    // internal variables cleared
    hcount      = (vluint16_t)hor_size;
    vcount      = (vluint16_t)ver_size;
    prev_clk    = (vluint8_t)0;
    prev_hs     = (vluint8_t)0;
    prev_vs     = (vluint8_t)0;
    dump_ctr    = (int)0;
    // synch
    v_sync_stage = e_Front;
    h_sync_stage = e_Front;
    v_sync_count = 0;
    h_sync_count = 0;
}

// Destructor
VideoOut::~VideoOut()
{

}

// evaluate : RGB with synchros
void VideoOut::eval_RGB_HV
(
    // Clock
    vluint8_t  clk,
    // Synchros
    vluint8_t  vs,
    vluint8_t  hs,
    // RGB colors
    vluint8_t  red,
    vluint8_t  green,
    vluint8_t  blue
)
{

    // Rising edge on clock
    if (clk && !prev_clk) {
        //printf("\nh stage %d, v stage %d, hs %d (prev:%d), vs %d (prev:%d), hcount %d, vcount %d, hsync_cnt:%d, vsync_cnt:%d\n",
        //       h_sync_stage,v_sync_stage,hs,prev_hs,vs,prev_vs,hcount,vcount,h_sync_count,v_sync_count);

        // Horizontal synch update
        bool h_sync_achieved = false;
        switch (h_sync_stage)
        {
        case e_Front:
            h_sync_count ++;
            if (h_sync_count == hfporch-1) {
                h_sync_stage = e_SynchPulseUp;
                h_sync_count = 0;
            }
            break;
        case e_SynchPulseUp:
            if ((hs == hs_pol) && (prev_hs != hs_pol)) {
                // raising edge on hs
                h_sync_stage = e_SynchPulseDown;
                if (dbg_on) printf(" Rising edge on HS");
            }
            break;
        case e_SynchPulseDown:
            if ((hs != hs_pol) && (prev_hs != hs_pol)) {
                // falling edge on hs
                h_sync_stage = e_Back;
                h_sync_count ++;
            }
            break;
        case e_Back:
            h_sync_count ++;
            if (h_sync_count == hbporch) {
                h_sync_stage = e_Done;
                h_sync_count = 0;
                hcount = 0;
                // just achived hsynch
                h_sync_achieved = true;
                // end of frame?
                if (vcount >= ver_size) {
                    // yes, trigger vsynch
                    vcount = 0;
                    h_sync_stage = e_Front;
                }
            }
            break;
        case e_Done: break;
        }

        // Vertical synch update, if horizontal synch achieved
        if (h_sync_achieved) {

            switch (v_sync_stage)
            {
            case e_Front:
                v_sync_count ++;
                if (v_sync_count == vfporch-1) {
                    v_sync_stage = e_SynchPulseUp;
                    v_sync_count = 0;
                }
                break;
            case e_SynchPulseUp:
                if ((vs == vs_pol) && (prev_vs != vs_pol)) {
                    // raising edge on vs
                    v_sync_stage = e_SynchPulseDown;
                    if (dbg_on) printf(" Rising edge on VS");
                }
                break;
            case e_SynchPulseDown:
                if ((vs != vs_pol) && (prev_vs != vs_pol)) {
                    // falling edge on vs
                    v_sync_stage = e_Back;
                    v_sync_count ++;

                }
                break;
            case e_Back:
                v_sync_count ++;
                if (v_sync_count == vbporch) {
                    vcount = 0;
                    v_sync_stage = e_Done;
                    v_sync_count = 0;
                    {
                        char num[64];
                        snprintf(num,64, "%04d", dump_ctr);
                        std::string tmp = filename + "_" + std::string(num) + ".tga";
                        // printf(" Save snapshot in file \"%s\"\n", tmp.c_str());
                        {
                          ImageRGB img;
                          img.pixels() = pixels;
                          saveImage(tmp.c_str(),&img);                        
                        }
                        dump_ctr++;
                    }
                }
                break;
            case e_Done: break;
            }

            prev_vs = vs;

        }

        // reset horizontal synchro
        if (h_sync_stage == e_Done) {
            if (hcount >= hor_size) {
                h_sync_stage = e_Front;
                vcount ++;
            }
        }
        // reset vertical synchro
        if (v_sync_stage == e_Done) {
            if (vcount >= ver_size) {
                h_sync_stage = e_Front;
                v_sync_stage = e_Front;
            }
        }

        // Grab active area
        if (v_sync_stage == e_Done && h_sync_stage == e_Done) {

            if (vcount < ver_size) {
                if (hcount < hor_size) {

                    uchar r = (red   & bit_mask) << (bit_shift);
                    uchar g = (green & bit_mask) << (bit_shift);
                    uchar b = (blue  & bit_mask) << (bit_shift);

                    if (hcount >= 0 && vcount >= 0 && hcount < pixels.xsize() && vcount < pixels.ysize()) {
                      pixels.at((int)(hcount), (int)(vcount)) = v3b(r,g,b);
                    // printf("*** [pixel write at %d,%d  R%dG%dB%d]\n",hcount,vcount,(int)r,(int)g,(int)b);
                    } else {
                      printf("*** [ERROR] out of bounds pixel write at %d,%d  R%dG%dB%d]\n",hcount,vcount,(int)r,(int)g,(int)b);
                    }
                }
            }
        }

        hcount ++;

        prev_hs = hs;

    }

    prev_clk = clk;

}

vluint16_t VideoOut::get_hcount()
{
    return hcount;
}

vluint16_t VideoOut::get_vcount()
{
    return vcount;
}

