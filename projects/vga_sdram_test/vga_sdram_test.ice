// ------------------------- 

$include('../common/video_sdram_main.ice')

// ------------------------- 

algorithm frame_drawer(
  output uint8 leds,
  sdio sd {
    output addr,
    output rw,
    output data_in,
    output in_valid,
    input  data_out,
    input  busy,
    input  out_valid,
  },
$$if HAS_COMPUTE_CLOCK then
  input  uint1  sdram_clock,
  input  uint1  sdram_reset,
$$end
  input  uint1  vsync,
  output uint1  fbuffer
) <autorun> {

  uint16 shift = 0;
  uint1  vsync_filtered = 0;
  
  subroutine clear(
    readwrites sd,
    input   uint1 buffer
  ) {
    uint9 pix_x   = 0;
    uint8 pix_y   = 0;
    uint8 pix_palidx = 0;
    	
    pix_y = 0;  
    while (pix_y < 200) {
      pix_x  = 0;
      while (pix_x < 320) {		
        // write to sdram
        while (1) {          
          if (sd.busy == 0) {        // not busy?
            sd.data_in    = pix_palidx;
            sd.addr       = {1b0,buffer,24b0} | (pix_x) | (pix_y << 9);             
            sd.in_valid   = 1; // go ahead!
            break;
          }
        }		
        pix_x = pix_x + 1;
      }
      pix_y = pix_y + 1;
    }
    return;
  }

  subroutine bands(
    reads   shift,
    readwrites sd,
    input   uint1 buffer
  ) {
    uint9 pix_x   = 0;
    uint8 pix_y   = 0;
    uint8 pix_palidx = 0;

    pix_y = 0;  
    while (pix_y < 200) {
      pix_x  = 0;
      while (pix_x < 320) {

        pix_palidx = pix_x + pix_y + shift;
        // write to sdram
        while (1) {
          if (sd.busy == 0) {        // not busy?
            sd.addr       = {1b0,buffer,24b0} | (pix_x) | (pix_y << 9); 
            sd.data_in    = pix_palidx;
            sd.in_valid = 1; // go ahead!
            break;
          }
        }
        
        pix_x = pix_x + 1;
      }
      pix_y = pix_y + 1;
    }
    return;
  }
  
  vsync_filtered ::= vsync;

  leds := 0;

  sd.in_valid   := 0; // maintain low (pulses high when needed)
  sd.rw         := 1; // always writes

  // clear SDRAM buffers
  () <- clear <- (0);
  () <- clear <- (1);
  () <- bands <- (0);
  () <- bands <- (1);

  while (1) {

    // draw a frame
    () <- bands <- (~fbuffer);
  
    // increment shift    
    shift = shift + 1;
    if (shift >= 320) {
      shift = 0;
    }
    
    // wait for frame to end
    while (vsync_filtered == 0) {}

    // swap buffers
    fbuffer = ~fbuffer;

  }
}

// ------------------------- 
