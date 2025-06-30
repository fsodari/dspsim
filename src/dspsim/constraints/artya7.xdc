# Clock signal
set_property -dict { PACKAGE_PIN E3    IOSTANDARD LVCMOS33 } [get_ports { arty_clk }]; #IO_L12P_T1_MRCC_35 Sch=gclk[100]
create_clock -add -name arty_clk -period 10.00 -waveform {0 5} [get_ports {arty_clk}];

set_property -dict { PACKAGE_PIN C2    IOSTANDARD LVCMOS33 } [get_ports { rst_n }]; #IO_L16P_T2_35 Sch=ck_rst

# PMOD Headers
## Pmod Header JA.
## Use for PSoC SPI and GPIO
set_property -dict { PACKAGE_PIN G13   IOSTANDARD LVCMOS33 } [get_ports { psoc_cs }]; #IO_0_15 Sch=ja[1]
set_property -dict { PACKAGE_PIN B11   IOSTANDARD LVCMOS33 } [get_ports { psoc_sclk }]; #IO_L4P_T0_15 Sch=ja[2]
set_property -dict { PACKAGE_PIN A11   IOSTANDARD LVCMOS33 } [get_ports { psoc_mosi }]; #IO_L4N_T0_15 Sch=ja[3]
set_property -dict { PACKAGE_PIN D12   IOSTANDARD LVCMOS33 } [get_ports { psoc_miso }]; #IO_L6P_T0_15 Sch=ja[4]
set_property -dict { PACKAGE_PIN D13   IOSTANDARD LVCMOS33 } [get_ports { psoc_mute }]; #IO_L6N_T0_VREF_15 Sch=ja[7]
#set_property -dict { PACKAGE_PIN B18   IOSTANDARD LVCMOS33 } [get_ports { VGA_R[1] }]; #IO_L10P_T1_AD11P_15 Sch=ja[8]
#set_property -dict { PACKAGE_PIN A18   IOSTANDARD LVCMOS33 } [get_ports { VGA_R[2] }]; #IO_L10N_T1_AD11N_15 Sch=ja[9]
#set_property -dict { PACKAGE_PIN K16   IOSTANDARD LVCMOS33 } [get_ports { VGA_R[3] }]; #IO_25_15 Sch=ja[10]

##Pmod Header JB
## Use for PSoC I2S and GPIO. Use a clock capable pin for psoc_mclk
set_property -dict { PACKAGE_PIN E15   IOSTANDARD LVCMOS33 } [get_ports { psoc_i2s_lrclk }]; #IO_L11P_T1_SRCC_15 Sch=jb_p[1]
set_property -dict { PACKAGE_PIN E16   IOSTANDARD LVCMOS33 } [get_ports { psoc_i2s_sclk }]; #IO_L11N_T1_SRCC_15 Sch=jb_n[1]
set_property -dict { PACKAGE_PIN D15   IOSTANDARD LVCMOS33 } [get_ports { psoc_i2s_mclk }]; #IO_L12P_T1_MRCC_15 Sch=jb_p[2]
set_property -dict { PACKAGE_PIN C15   IOSTANDARD LVCMOS33 } [get_ports { psoc_i2s_sdo }]; #IO_L12N_T1_MRCC_15 Sch=jb_n[2]
set_property -dict { PACKAGE_PIN J17   IOSTANDARD LVCMOS33 } [get_ports { psoc_i2s_sdi }]; #IO_L23P_T3_FOE_B_15 Sch=jb_p[3]
# set_property -dict { PACKAGE_PIN J18   IOSTANDARD LVCMOS33 } [get_ports { VGA_B[1] }]; #IO_L23N_T3_FWE_B_15 Sch=jb_n[3]
# set_property -dict { PACKAGE_PIN K15   IOSTANDARD LVCMOS33 } [get_ports { VGA_B[2] }]; #IO_L24P_T3_RS1_15 Sch=jb_p[4]
# set_property -dict { PACKAGE_PIN J15   IOSTANDARD LVCMOS33 } [get_ports { VGA_B[3] }]; #IO_L24N_T3_RS0_15 Sch=jb_n[4]

# 12.288Mhz
# create_clock -add -name psoc_i2s_mclk -period 81.38 [get_ports {psoc_i2s_mclk}];
# 18.432Mhz
create_clock -add -name psoc_i2s_mclk -period 54.253 [get_ports {psoc_i2s_mclk}];
# 36.864Mhz
# create_clock -add -name psoc_i2s_mclk -period 27.1267 [get_ports {psoc_i2s_mclk}];

# The audio clock is asynchronous to the system clk. Timing fails without this.
set_clock_groups -asynchronous -group [get_clocks arty_clk] -group [get_clocks psoc_i2s_mclk]

##Pmod Header JC

# set_property -dict { PACKAGE_PIN U12   IOSTANDARD LVCMOS33 } [get_ports { VGA_G[0] }]; #IO_L20P_T3_A08_D24_14 Sch=jc_p[1]
# set_property -dict { PACKAGE_PIN V12   IOSTANDARD LVCMOS33 } [get_ports { VGA_G[1] }]; #IO_L20N_T3_A07_D23_14 Sch=jc_n[1]
# set_property -dict { PACKAGE_PIN V10   IOSTANDARD LVCMOS33 } [get_ports { VGA_G[2] }]; #IO_L21P_T3_DQS_14 Sch=jc_p[2]
# set_property -dict { PACKAGE_PIN V11   IOSTANDARD LVCMOS33 } [get_ports { VGA_G[3] }]; #IO_L21N_T3_DQS_A06_D22_14 Sch=jc_n[2]
# set_property -dict { PACKAGE_PIN U14   IOSTANDARD LVCMOS33 } [get_ports { VGA_HS_O }]; #IO_L22P_T3_A05_D21_14 Sch=jc_p[3]
# set_property -dict { PACKAGE_PIN V14   IOSTANDARD LVCMOS33 } [get_ports { VGA_VS_O }]; #IO_L22N_T3_A04_D20_14 Sch=jc_n[3]
#set_property -dict { PACKAGE_PIN T13   IOSTANDARD LVCMOS33 } [get_ports { jc[6] }]; #IO_L23P_T3_A03_D19_14 Sch=jc_p[4]
#set_property -dict { PACKAGE_PIN U13   IOSTANDARD LVCMOS33 } [get_ports { jc[7] }]; #IO_L23N_T3_A02_D18_14 Sch=jc_n[4]

##Pmod Header JD

set_property -dict { PACKAGE_PIN D4    IOSTANDARD LVCMOS33 } [get_ports { dac_i2s_mclk }]; #IO_L11N_T1_SRCC_35 Sch=jd[1]
set_property -dict { PACKAGE_PIN D3    IOSTANDARD LVCMOS33 } [get_ports { dac_i2s_lrclk }]; #IO_L12N_T1_MRCC_35 Sch=jd[2]
set_property -dict { PACKAGE_PIN F4    IOSTANDARD LVCMOS33 } [get_ports { dac_i2s_sclk }]; #IO_L13P_T2_MRCC_35 Sch=jd[3]
set_property -dict { PACKAGE_PIN F3    IOSTANDARD LVCMOS33 } [get_ports { dac_i2s_sdo }]; #IO_L13N_T2_MRCC_35 Sch=jd[4]
set_property -dict { PACKAGE_PIN E2    IOSTANDARD LVCMOS33 } [get_ports { adc_i2s_mclk }]; #IO_L14P_T2_SRCC_35 Sch=jd[7]
set_property -dict { PACKAGE_PIN D2    IOSTANDARD LVCMOS33 } [get_ports { adc_i2s_lrclk }]; #IO_L14N_T2_SRCC_35 Sch=jd[8]
set_property -dict { PACKAGE_PIN H2    IOSTANDARD LVCMOS33 } [get_ports { adc_i2s_sclk }]; #IO_L15P_T2_DQS_35 Sch=jd[9]
set_property -dict { PACKAGE_PIN G2    IOSTANDARD LVCMOS33 } [get_ports { adc_i2s_sdi }]; #IO_L15N_T2_DQS_35 Sch=jd[10]


#Switches
set_property -dict { PACKAGE_PIN A8    IOSTANDARD LVCMOS33 } [get_ports { sw[0] }]; #IO_L12N_T1_MRCC_16 Sch=sw[0]
set_property -dict { PACKAGE_PIN C11   IOSTANDARD LVCMOS33 } [get_ports { sw[1] }]; #IO_L13P_T2_MRCC_16 Sch=sw[1]
set_property -dict { PACKAGE_PIN C10   IOSTANDARD LVCMOS33 } [get_ports { sw[2] }]; #IO_L13N_T2_MRCC_16 Sch=sw[2]
set_property -dict { PACKAGE_PIN A10   IOSTANDARD LVCMOS33 } [get_ports { sw[3] }]; #IO_L14P_T2_SRCC_16 Sch=sw[3]

# Buttons
set_property -dict { PACKAGE_PIN D9    IOSTANDARD LVCMOS33 } [get_ports { btn[0] }]; #IO_L6N_T0_VREF_16 Sch=btn[0]
set_property -dict { PACKAGE_PIN C9    IOSTANDARD LVCMOS33 } [get_ports { btn[1] }]; #IO_L11P_T1_SRCC_16 Sch=btn[1]
set_property -dict { PACKAGE_PIN B9    IOSTANDARD LVCMOS33 } [get_ports { btn[2] }]; #IO_L11N_T1_SRCC_16 Sch=btn[2]
set_property -dict { PACKAGE_PIN B8    IOSTANDARD LVCMOS33 } [get_ports { btn[3] }]; #IO_L12P_T1_MRCC_16 Sch=btn[3]

# LEDs
set_property -dict { PACKAGE_PIN H5    IOSTANDARD LVCMOS33 } [get_ports { led[0] }]; #IO_L24N_T3_35 Sch=led[4]
set_property -dict { PACKAGE_PIN J5    IOSTANDARD LVCMOS33 } [get_ports { led[1] }]; #IO_25_35 Sch=led[5]
set_property -dict { PACKAGE_PIN T9    IOSTANDARD LVCMOS33 } [get_ports { led[2] }]; #IO_L24P_T3_A01_D17_14 Sch=led[6]
set_property -dict { PACKAGE_PIN T10   IOSTANDARD LVCMOS33 } [get_ports { led[3] }]; #IO_L24N_T3_A00_D16_14 Sch=led[7]

# RGB LEDs
set_property -dict { PACKAGE_PIN E1    IOSTANDARD LVCMOS33 } [get_ports { rgb[0][2] }]; #IO_L18N_T2_35 Sch=led0_b
set_property -dict { PACKAGE_PIN F6    IOSTANDARD LVCMOS33 } [get_ports { rgb[0][1] }]; #IO_L19N_T3_VREF_35 Sch=led0_g
set_property -dict { PACKAGE_PIN G6    IOSTANDARD LVCMOS33 } [get_ports { rgb[0][0] }]; #IO_L19P_T3_35 Sch=led0_r
set_property -dict { PACKAGE_PIN G4    IOSTANDARD LVCMOS33 } [get_ports { rgb[1][2] }]; #IO_L20P_T3_35 Sch=led1_b
set_property -dict { PACKAGE_PIN J4    IOSTANDARD LVCMOS33 } [get_ports { rgb[1][1] }]; #IO_L21P_T3_DQS_35 Sch=led1_g
set_property -dict { PACKAGE_PIN G3    IOSTANDARD LVCMOS33 } [get_ports { rgb[1][0] }]; #IO_L20N_T3_35 Sch=led1_r
set_property -dict { PACKAGE_PIN H4    IOSTANDARD LVCMOS33 } [get_ports { rgb[2][2] }]; #IO_L21N_T3_DQS_35 Sch=led2_b
set_property -dict { PACKAGE_PIN J2    IOSTANDARD LVCMOS33 } [get_ports { rgb[2][1] }]; #IO_L22N_T3_35 Sch=led2_g
set_property -dict { PACKAGE_PIN J3    IOSTANDARD LVCMOS33 } [get_ports { rgb[2][0] }]; #IO_L22P_T3_35 Sch=led2_r
set_property -dict { PACKAGE_PIN K2    IOSTANDARD LVCMOS33 } [get_ports { rgb[3][2] }]; #IO_L23P_T3_35 Sch=led3_b
set_property -dict { PACKAGE_PIN H6    IOSTANDARD LVCMOS33 } [get_ports { rgb[3][1] }]; #IO_L24P_T3_35 Sch=led3_g
set_property -dict { PACKAGE_PIN K1    IOSTANDARD LVCMOS33 } [get_ports { rgb[3][0] }]; #IO_L23N_T3_35 Sch=led3_r

# Multiboot settings
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
# set_property BITSTREAM.CONFIG.CONFIGRATE 66 [current_design]
set_property BITSTREAM.CONFIG.SPI_32BIT_ADDR YES [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.CONFIG.M1PIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.M2PIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.M0PIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.USR_ACCESS TIMESTAMP [current_design]
# set_property BITSTREAM.CONFIG.CONFIGFALLBACK ENABLE [current_design]
# set_property BITSTREAM.CONFIG.TIMER_CFG 66000000 [current_design]
