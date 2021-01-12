// SL 2020-12-02 @sylefeb
// ------------------------- 

$$if SIMULATION then
$$verbose = nil
$$end

// pre-compilation script, embeds compile code within sdcard image
$$dofile('pre_include_asm.lua')
$$if not SIMULATION then
$$  init_data_bytes = math.max(init_data_bytes,(1<<21)) -- we load 2 MB to be sure we can append stuff
$$end

// default palette
$$palette = {}
$$for i=1,256 do
$$  palette[i] = (i) | (((i<<1)&255)<<8) | (((i<<2)&255)<<16)
$$end
$$ palette[256] = 255 | (255<<8) | (255<<16)

$$if SIMULATION then
$$  frame_drawer_at_sdram_speed = true
$$else
$$  fast_compute = true
$$end

$include('../common/video_sdram_main.ice')

$include('ram-ice-v.ice')
$include('bram_ram_32bits.ice')
$include('sdram_ram_32bits.ice')

// ------------------------- 

$$div_width=24
$include('../common/divint_std.ice')

algorithm edge_intersect(
  input  uint16  y,
  input  uint16 x0, // 10+6 fixed point
  input  uint16 y0, // assumes y0 < y1
  input  uint16 x1,
  input  uint16 y1,
  input  uint1  prepare,
  output uint24 xi,
  output uint1  intersects
) <autorun> {
$$if SIMULATION then
  uint16 cycle = 0;
  uint16 cycle_last = 0;
$$end

  div24 div(ret :> interp);

  uint1  in_edge  ::= (y0 < y && y1 >= y) || (y1 < y && y0 >= y);
  int16  interp     = uninitialized;
  uint16 last_y     = uninitialized;

  uint1  do_prepare = 0;

  intersects := in_edge;

 always {
$$if SIMULATION then
    cycle = cycle + 1;
$$end
    do_prepare = prepare | do_prepare;
 }

  while (1) {

    if (do_prepare) {
$$if SIMULATION then
      cycle_last = cycle;
$$end
      do_prepare = 0;
      div <- (  __signed(x1-x0)<<8 ,  __signed(y1-y0)>>>6 );
      last_y = y0;
      xi     = x0 << 8;
__display("[prepared]");
    } else {
      if (y == last_y + $1<<6$) {
        xi = xi + interp;
        last_y = y;
$$if SIMULATION then
__display("  next [%d cycles] : interp:%d xi:%d)",cycle-cycle_last,interp,xi>>14);
$$end
      }
    }
  }
}

// ------------------------- 

algorithm frame_drawer(
  sdram_user    sd,
  input  uint1  sdram_clock,
  input  uint1  sdram_reset,
  input  uint1  vsync,
  output uint1  fbuffer,
  output uint8  leds,
  simple_dualport_bram_port1 palette,
) <autorun> {

  rv32i_ram_io sdram;
  // sdram io
  sdram_ram_32bits bridge<@sdram_clock,!sdram_reset>(
    sdr <:> sd,
    r32 <:> sdram,
  );

  rv32i_ram_io mem;
  uint26 predicted_addr = uninitialized;

  // bram io
  bram_ram_32bits bram_ram(
    pram <:> mem,
    predicted_addr <: predicted_addr,
  );

  uint1  cpu_reset      = 1;
  uint26 cpu_start_addr = 26h0000000;
  uint3  cpu_id         = 0;
  
  // cpu 
  rv32i_cpu cpu<!cpu_reset>(
    boot_at  <:  cpu_start_addr,
    predicted_addr :> predicted_addr,
    cpu_id   <:  cpu_id,
    ram      <:> mem
  );

  // fun
  uint16  y  = uninitialized;
  uint16  x0 = uninitialized; // 10+6 fixed point
  uint16  y0 = uninitialized;
  uint16  x1 = uninitialized;
  uint16  y1 = uninitialized;
  int16   xi = uninitialized;
  uint1   intersects = uninitialized;
  uint1   prepare = uninitialized;
  edge_intersect ei(<:auto:>);

  fbuffer              := 0;
  sdram.rw             := 0;
  palette.wenable1     := 0;
  sdram.in_valid       := 0;
  prepare              := 0;

  while (1) {
    cpu_reset = 0;

    if (mem.in_valid & mem.rw) {
      switch (mem.addr[28,4]) {
        case 4b1000: {
          // __display("palette %h = %h",mem.addr[2,8],mem.data_in[0,24]);
          palette.addr1    = mem.addr[2,8];
          palette.wdata1   = mem.data_in[0,24];
          palette.wenable1 = 1;
        }
        case 4b0100: {
          // __display("SDRAM %h = %h",mem.addr[0,26],mem.data_in);
          sdram.data_in  = mem.data_in;
          sdram.wmask    = mem.wmask;
          sdram.addr     = mem.addr[0,26];
          sdram.rw       = 1;
          sdram.in_valid = 1;
        }
        case 4b0010: {
          __display("LEDs = %h",mem.data_in[0,8]);
          leds = mem.data_in[0,8];
        }
        case 4b0001: {
          // __display("triangle (%b) = %d %d",mem.addr[2,3],mem.data_in[0,16]>>6,mem.data_in[16,16]>>6);
          switch (mem.addr[2,3]) {
            case 3b001: { x0 = mem.data_in[0,16]; y0 = mem.data_in[16,16]; }
            case 3b010: { x1 = mem.data_in[0,16]; y1 = mem.data_in[16,16]; prepare = 1;}
            case 3b100: { y  = mem.data_in[0,16]; }
            default: { }
          }
        }
        default: { }
      }
    }
    
  }
}

// ------------------------- 
