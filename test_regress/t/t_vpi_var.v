// DESCRIPTION: Verilator: Verilog Test module
//
// Copyright 2010 by Wilson Snyder. This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

`ifdef USE_VPI_NOT_DPI
//We call it via $c so we can verify DPI isn't required - see bug572
`else
import "DPI-C" context function int mon_check();
`endif

module t (/*AUTOARG*/
   // Outputs
   x,
   // Inputs
   clk, a
   );

`ifdef VERILATOR
`systemc_header
extern "C" int mon_check();
`verilog
`endif

   input clk;

   input [7:0] a;
   output reg [7:0] x;

   reg          onebit          /*verilator public_flat_rw @(posedge clk) */;
   reg [2:1]    twoone          /*verilator public_flat_rw @(posedge clk) */;
   reg [2:1]    fourthreetwoone[4:3] /*verilator public_flat_rw @(posedge clk) */;
   reg LONGSTART_a_very_long_name_which_will_get_hashed_a_very_long_name_which_will_get_hashed_a_very_long_name_which_will_get_hashed_a_very_long_name_which_will_get_hashed_LONGEND /*verilator public_flat_rw*/;

   // verilator lint_off ASCRANGE
   reg [0:61]   quads[2:3]      /*verilator public_flat_rw @(posedge clk) */;
   // verilator lint_on ASCRANGE

   reg [31:0]      count        /*verilator public_flat */;
   reg [31:0]      half_count   /*verilator public_flat_rd */;
   reg [31:0]      delayed      /*verilator public_flat_rw */;
   reg [31:0]      delayed_mem [16] /*verilator public_flat_rw */;

   reg [7:0]       text_byte    /*verilator public_flat_rw @(posedge clk) */;
   reg [15:0]      text_half    /*verilator public_flat_rw @(posedge clk) */;
   reg [31:0]      text_word    /*verilator public_flat_rw @(posedge clk) */;
   reg [63:0]      text_long    /*verilator public_flat_rw @(posedge clk) */;
   reg [511:0]     text         /*verilator public_flat_rw @(posedge clk) */;
   reg [2047:0]    too_big      /*verilator public_flat_rw @(posedge clk) */;

   integer        status;

   real           real1          /*verilator public_flat_rw */;
   string         str1           /*verilator public_flat_rw */;
   // specifically public and not public_flat_rw here so as to induce the C++
   // keyword collision
   localparam int nullptr        /*verilator public */ = 123;

   sub sub();

   // Test loop
   initial begin
      count = 0;
      delayed = 0;
      onebit = 1'b0;
      fourthreetwoone[3] = 0; // stop icarus optimizing away
      text_byte = "B";
      text_half = "Hf";
      text_word = "Word";
      text_long = "Long64b";
      text = "Verilog Test module";
      too_big = "some text";

      real1 = 1.0;
      str1 = "hello";

`ifdef VERILATOR
      status = $c32("mon_check()");
`endif
`ifdef IVERILOG
      status = $mon_check();
`endif
`ifndef USE_VPI_NOT_DPI
      status = mon_check();
`endif
      if (status!=0) begin
         $write("%%Error: t_vpi_var.cpp:%0d: C Test failed\n", status);
         $stop;
      end
      $write("%%Info: Checking results\n");
      if (onebit != 1'b1) $stop;
      if (quads[2] != 62'h12819213_abd31a1c) $stop;
      if (quads[3] != 62'h1c77bb9b_3784ea09) $stop;
      if (text_byte != "A") $stop;
      if (text_half != "T2") $stop;
      if (text_word != "Tree") $stop;
      if (text_long != "44Four44") $stop;
      if (text != "lorem ipsum") $stop;
      if (str1 != "something a lot longer than hello") $stop;
      if (real1 > 123456.7895 || real1 < 123456.7885 ) $stop;
   end

   always @(posedge clk) begin
      count <= count + 2;
      if (count[1])
        half_count <= half_count + 2;

      if (count == 1000) begin
         if (delayed != 123) $stop;
         if (delayed_mem[7] != 456) $stop;
         $write("*-* All Finished *-*\n");
         $finish;
      end
   end

   genvar i;
   generate
   for (i=1; i<=6; i=i+1) begin : arr
     arr #(.LENGTH(i)) arr();
   end
   endgenerate

   genvar k;
   generate
   for (k=1; k<=6; k=k+1) begin : subs
      sub subsub();
   end
   endgenerate

endmodule : t

module sub;
   reg subsig1 /*verilator public_flat_rw*/;
   reg subsig2 /*verilator public_flat_rd*/;
`ifdef IVERILOG
   // stop icarus optimizing signals away
   wire redundant = subsig1 | subsig2;
`endif
endmodule : sub

module arr;

   parameter LENGTH = 1;

   reg [LENGTH-1:0] sig /*verilator public_flat_rw*/;
   reg [LENGTH-1:0] rfr /*verilator public_flat_rw*/;

   reg            check /*verilator public_flat_rw*/;
   reg          verbose /*verilator public_flat_rw*/;

   initial begin
      sig = {LENGTH{1'b0}};
      rfr = {LENGTH{1'b0}};
   end

   always @(posedge check) begin
     if (verbose) $display("%m : %x %x", sig, rfr);
     if (check && sig != rfr) $stop;
     check <= 0;
   end

endmodule : arr
