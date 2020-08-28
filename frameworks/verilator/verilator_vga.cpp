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
// SL 2019-09-23

#include "Vvga.h"
#include <iostream>

#include "VgaChip.h"
#include "sdr_sdram.h"

unsigned int main_time = 0;
double sc_time_stamp()
{
  return main_time;
}

int main(int argc,char **argv)
{
  Verilated::commandArgs(argc,argv);
  
  VgaChip *vga_chip = new VgaChip();

  char foo[1<<19]; // DEBUG FIXME: there is an access violation that makes this necessary. I have not been able to track it down so far!! Terrible.
  
  Vvga    *vga_test = new Vvga();

  // setup for a mt48lc32m8a2 // 8 M x 8 bits x 4 banks (256 MB),
  // 67,108,864-bit banks is organized as 8192 rows by 1024 columns by 8 bits
  // this matches the Mojo Alchitry board with SDRAM shield
  vluint8_t sdram_flags = FLAG_DATA_WIDTH_8; // | FLAG_BANK_INTERLEAVING | FLAG_BIG_ENDIAN;
  SDRAM* sdr  = new SDRAM(13 /*8192*/, 10 /*1024*/, sdram_flags, NULL); 
                                                             // "sdram.txt");
  vluint64_t sdram_dq = 0;
  
  vga_test->clk = 0;

  while (!Verilated::gotFinish()) {

    vga_test->clk = 1 - vga_test->clk;

    vga_test->eval();

    sdr->eval(main_time,
              vga_test->sdram_clock, 1,
              vga_test->sdram_cs,  vga_test->sdram_ras, vga_test->sdram_cas, vga_test->sdram_we,
              vga_test->sdram_ba,  vga_test->sdram_a,
              vga_test->sdram_dqm, (vluint64_t)vga_test->sdram_dq_o, sdram_dq);
    vga_test->sdram_dq_i = (vga_test->sdram_dq_en) ? vga_test->sdram_dq_o : sdram_dq;
    
    vga_chip->eval(vga_test->video_clock,vga_test->video_vs,vga_test->video_hs,vga_test->video_r,vga_test->video_g,vga_test->video_b);

    main_time ++;
  }

  return 0;
}

