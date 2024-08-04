#include <stdio.h>

#include "driver/i2c_master.h"
// Include I2S legacy driver
// FIXME: move to newer std driver?
#include <driver/i2s.h>

#include "WM8960.h"

#define I2C_PORT_NUM 0
#define I2C_SCL_GPIO_NUM GPIO_NUM_4
#define I2C_SDA_GPIO_NUM GPIO_NUM_5

i2c_master_bus_config_t i2c_mst_config = {
    .i2c_port = I2C_PORT_NUM,
    .sda_io_num = I2C_SDA_GPIO_NUM,
    .scl_io_num = I2C_SCL_GPIO_NUM,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};



i2c_master_bus_handle_t bus_handle;

// Connections to I2S
#define I2S_WS 14 // DACLRC / ADCLRC
#define I2S_SD 11 // ADCDAT
#define I2S_SDO 12 // DACDAT
#define I2S_SCK 13 // BCLK

// Use I2S Processor 0
#define I2S_PORT I2S_NUM_0

// Define input buffer length
#define bufferLen 512
int16_t sBuffer[bufferLen];

wm8960_inst_t codec0;

void codec_setup()
{

  // General setup needed
  wm8960_enableVREF(&codec0);
  wm8960_enableVMID(&codec0);

  // Setup signal flow to the ADC

  wm8960_enableLMIC(&codec0);
  wm8960_enableRMIC(&codec0);

  // Connect from INPUT1 to "n" (aka inverting) inputs of PGAs.
  wm8960_connectLMN1(&codec0);
  wm8960_connectRMN1(&codec0);

  // Disable mutes on PGA inputs (aka INTPUT1)
  wm8960_disableLINMUTE(&codec0);
  wm8960_disableRINMUTE(&codec0);

  // Set pga volumes
  wm8960_setLINVOLDB(&codec0, 0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)
  wm8960_setRINVOLDB(&codec0, 0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)

  // Set input boosts to get inputs 1 to the boost mixers
  wm8960_setLMICBOOST(&codec0, WM8960_MIC_BOOST_GAIN_0DB);
  wm8960_setRMICBOOST(&codec0, WM8960_MIC_BOOST_GAIN_0DB);

  // Connect from MIC inputs (aka pga output) to boost mixers
  wm8960_connectLMIC2B(&codec0);
  wm8960_connectRMIC2B(&codec0);

  // Enable boost mixers
  wm8960_enableAINL(&codec0);
  wm8960_enableAINR(&codec0);

  // Disconnect LB2LO (booster to output mixer (analog bypass)
  // For this example, we are going to pass audio throught the ADC and DAC
  wm8960_disableLB2LO(&codec0);
  wm8960_disableRB2RO(&codec0);

  // Connect from DAC outputs to output mixer
  wm8960_enableLD2LO(&codec0);
  wm8960_enableRD2RO(&codec0);

  // Set gainstage between booster mixer and output mixer
  // For this loopback example, we are going to keep these as low as they go
  wm8960_setLB2LOVOL(&codec0, WM8960_OUTPUT_MIXER_GAIN_NEG_21DB); 
  wm8960_setRB2ROVOL(&codec0, WM8960_OUTPUT_MIXER_GAIN_NEG_21DB);

  // Enable output mixers
  wm8960_enableLOMIX(&codec0);
  wm8960_enableROMIX(&codec0);

  // CLOCK STUFF, These settings will get you 44.1KHz sample rate, and class-d 
  // freq at 705.6kHz
  wm8960_enablePLL(&codec0); // Needed for class-d amp clock
  wm8960_setPLLPRESCALE(&codec0, WM8960_PLLPRESCALE_DIV_2);
  wm8960_setSMD(&codec0, WM8960_PLL_MODE_FRACTIONAL);
  wm8960_setCLKSEL(&codec0, WM8960_CLKSEL_PLL);
  wm8960_setSYSCLKDIV(&codec0, WM8960_SYSCLK_DIV_BY_2);
  wm8960_setBCLKDIV(&codec0, 4);
  wm8960_setDCLKDIV(&codec0, WM8960_DCLKDIV_16);
  wm8960_setPLLN(&codec0, 7);
  wm8960_setPLLK(&codec0, 0x86, 0xC2, 0x26); // PLLK=86C226h
  //wm8960_setADCDIV(codec00); // Default is 000 (what we need for 44.1KHz)
  //wm8960_setDACDIV(codec00); // Default is 000 (what we need for 44.1KHz)
  wm8960_setWL(&codec0, WM8960_WL_16BIT);

  wm8960_enablePeripheralMode(&codec0);
  //wm8960_enableMasterMode(&codec0);
  //wm8960_setALRCGPIO(&codec0); // Note, should not be changed while ADC is enabled.

  // Enable ADCs and DACs
  wm8960_enableAdcLeft(&codec0);
  wm8960_enableAdcRight(&codec0);
  wm8960_enableDacLeft(&codec0);
  wm8960_enableDacRight(&codec0);
  wm8960_disableDacMute(&codec0);

  //wm8960_enableLoopBack(&codec0); // Loopback sends ADC data directly into DAC
  wm8960_disableLoopBack(&codec0);

  // Default is "soft mute" on, so we must disable mute to make channels active
  wm8960_disableDacMute(&codec0); 

  wm8960_enableHeadphones(&codec0);
  wm8960_enableOUT3MIX(&codec0); // Provides VMID as buffer for headphone ground

  printf("Volume set to +0dB\n");
  wm8960_setHeadphoneVolumeDB(&codec0, -32.0);

  printf("&codec0 Setup complete. Listen to left/right INPUT1 on Headphone outputs.\n");
}

void i2s_install() {
  // Set up I2S Processor configuration
  const i2s_driver_config_t i2s_config = {
    .mode = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = 16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  // Set I2S pin configuration
  const i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_SDO,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}

void app_main(void)
{
  // SETUP
  printf("Example 8 - I2S Passthough\n");
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

  wm8960_init(&codec0);
  if (wm8960_begin(&codec0, bus_handle) == false) //Begin communication over I2C
  {
    printf("The device did not respond. Please check wiring.\n");
    abort();
  }
  printf("Device is connected properly.\n");

  codec_setup();
  printf("Codec setup done \n");
  // Set up I2S
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  // loop
  while (true) {
    // Get I2S data and place in data buffer
    size_t bytesIn = 0;
    size_t bytesOut = 0;
    esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

    if (result == ESP_OK)
    {
      // Send what we just received back to the &codec0
      esp_err_t result_w = i2s_write(I2S_PORT, &sBuffer, bytesIn, &bytesOut, portMAX_DELAY);

      // If there was an I2S write error, let us know on the serial terminal
      if (result_w != ESP_OK)
      {
        printf("I2S write error.\n");
      }
    }
    else {
      printf("I2S read error.\n");
    }
    // DelayMicroseconds(300); // Only hear to demonstrate how much time you have 
    // to do things.
    // Do not do much in this main loop, or the audio won't pass through correctly.
    // With default settings (64 samples in buffer), you can spend up to 300 
    // microseconds doing something in between passing each buffer of data
    // You can tweak the buffer length to get more time if you need it.
    // When bufferlength is 64, then you get ~300 microseconds
    // When bufferlength is 128, then you get ~600 microseconds
    // Note, as you increase bufferlength, then you are increasing latency between 
    // ADC input to DAC output.
    // Latency may or may not be desired, depending on the project.
  }
}