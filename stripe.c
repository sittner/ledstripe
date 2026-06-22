#include "stripe.h"

#include <xmc_gpio.h>
#include <xmc_spi.h>
#include <xmc_dma.h>
#include <xmc_ccu4.h>
#include <xmc_eru.h>


// IMPORTANT:

// We need two external interconnects:
// P0.11 -> P1.2 to feed the SPI SCLK signal to ERU
// P0.15 -> P0.3 to feed the PWM LO signal to ERU

// P1.15 (SPI DOUT) pin is used simultaneously as input for ERU

// -------------------------------------------------------------------
// --- GPIO configs
// -------------------------------------------------------------------

static const XMC_GPIO_CONFIG_t gpio_input_tristate_config = {
  .mode = XMC_GPIO_MODE_INPUT_TRISTATE,
};

static const XMC_GPIO_CONFIG_t gpio_output_alt2_config = {
  .mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT2,
  .output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SOFT_EDGE
};

static const XMC_GPIO_CONFIG_t gpio_output_alt3_config = {
  .mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT3,
  .output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SOFT_EDGE
};

static const XMC_GPIO_CONFIG_t gpio_output_alt4_config = {
  .mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT4,
  .output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SOFT_EDGE
};

// -------------------------------------------------------------------
// --- SPI part
// -------------------------------------------------------------------

#define SPI_TX   P1_15
#define SPI_SCLK P0_11

static const XMC_SPI_CH_CONFIG_t spi_config =
{
  .baudrate = 800000U,
  .bus_mode = XMC_SPI_CH_BUS_MODE_MASTER,
};

static const XMC_DMA_CH_CONFIG_t spi_tx_dma_config =
{
  .dst_addr = (uint32_t)&(XMC_SPI1_CH0->TBUF[0]),
  .src_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_8,
  .dst_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_8,
  .src_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_INCREMENT,
  .dst_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE,
  .src_burst_length = XMC_DMA_CH_BURST_LENGTH_1,
  .dst_burst_length = XMC_DMA_CH_BURST_LENGTH_1,
  .transfer_flow = XMC_DMA_CH_TRANSFER_FLOW_M2P_DMA,
  .transfer_type = XMC_DMA_CH_TRANSFER_TYPE_SINGLE_BLOCK,
  .dst_handshaking = XMC_DMA_CH_DST_HANDSHAKING_HARDWARE,
  .dst_peripheral_request = DMA0_PERIPHERAL_REQUEST_USIC1_SR0_0,
  .enable_interrupt = true,
};

static void spi_init(void)
{
  // initialize GPIOs
  XMC_GPIO_Init(SPI_TX, &gpio_output_alt4_config);
  XMC_GPIO_Init(SPI_SCLK, &gpio_output_alt2_config);

  // setup DMA
  XMC_DMA_Init(XMC_DMA0);
  XMC_DMA_CH_Init(XMC_DMA0, 0, &spi_tx_dma_config);

  // initialize SPI parameters
  XMC_SPI_CH_Init(XMC_SPI1_CH0, &spi_config);
  XMC_SPI_CH_SetWordLength(XMC_SPI1_CH0, 8);
  XMC_SPI_CH_SetFrameLength(XMC_SPI1_CH0, 64);
  XMC_SPI_CH_SetBitOrderMsbFirst(XMC_SPI1_CH0);

  // 50 us inter frame delay
  XMC_SPI_CH_SetInterwordDelay(XMC_SPI1_CH0, 50000);

  // disable slave (and clock) if no data is written to TBUF
  XMC_SPI_CH_DisableFEM(XMC_SPI1_CH0);

  // enable DMA trigger event
  XMC_USIC_CH_SetInterruptNodePointer(XMC_SPI1_CH0, XMC_USIC_CH_INTERRUPT_NODE_POINTER_TRANSMIT_SHIFT, 0);
  XMC_USIC_CH_EnableEvent(XMC_SPI1_CH0, XMC_USIC_CH_EVENT_TRANSMIT_SHIFT);

  // start SPI controller
  XMC_SPI_CH_Start(XMC_SPI1_CH0);
}

// -------------------------------------------------------------------
// --- ERU part
// -------------------------------------------------------------------

#define ERU_SCLK_IN   P1_2
#define ERU_PWM_LO_IN P0_3
#define ERU_DATA_OUT  P5_2

static const XMC_ERU_ETL_CONFIG_t etl_data_in_config = {
   .input_a = ERU1_ETL1_INPUTA_P1_15,
   .source = XMC_ERU_ETL_SOURCE_A,
   .status_flag_mode = true,
   .edge_detection = XMC_ERU_ETL_EDGE_DETECTION_RISING,
};

static const XMC_ERU_ETL_CONFIG_t etl_sclk_in_config = {
   .input_b = ERU1_ETL2_INPUTB_P1_2,
   .source = XMC_ERU_ETL_SOURCE_B,
   .status_flag_mode = true,
   .edge_detection = XMC_ERU_ETL_EDGE_DETECTION_RISING,
};

static const XMC_ERU_OGU_CONFIG_t ogu_pwm_hi_config = {
   .enable_pattern_detection = true,
   .pattern_detection_input = XMC_ERU_OGU_PATTERN_DETECTION_INPUT2 | XMC_ERU_OGU_PATTERN_DETECTION_INPUT1
};

static const XMC_ERU_OGU_CONFIG_t ogu_pwm_lo_config = {
   .enable_pattern_detection = true,
   .pattern_detection_input = XMC_ERU_OGU_PATTERN_DETECTION_INPUT2
};

static const XMC_ERU_ETL_CONFIG_t etl_data_out_config = {
   .input_a = ERU1_ETL3_INPUTA_CCU40_ST3,
   .input_b = ERU1_ETL3_INPUTB_P0_3,
   .source = XMC_ERU_ETL_SOURCE_A_OR_B,
   .status_flag_mode = true,
   .edge_detection = XMC_ERU_ETL_EDGE_DETECTION_RISING,
};

static const XMC_ERU_OGU_CONFIG_t ogu_data_out_config = {
   .enable_pattern_detection = true,
   .pattern_detection_input = XMC_ERU_OGU_PATTERN_DETECTION_INPUT3
};

static void eru_init(void)
{
  // initialize GPIOs
  XMC_GPIO_Init(ERU_SCLK_IN, &gpio_input_tristate_config);
  XMC_GPIO_Init(ERU_PWM_LO_IN, &gpio_input_tristate_config);
  XMC_GPIO_Init(ERU_DATA_OUT, &gpio_output_alt4_config);

  // initialize PWM trigger
  XMC_ERU_ETL_Init(ERU1_ETL1, &etl_data_in_config);
  XMC_ERU_ETL_Init(ERU1_ETL2, &etl_sclk_in_config);
  XMC_ERU_OGU_Init(ERU1_OGU0, &ogu_pwm_hi_config);
  XMC_ERU_OGU_Init(ERU1_OGU1, &ogu_pwm_lo_config);

  // initialize signal merger
  XMC_ERU_ETL_Init(ERU1_ETL3, &etl_data_out_config);
  XMC_ERU_OGU_Init(ERU1_OGU2, &ogu_data_out_config);
}

// -------------------------------------------------------------------
// --- PWM part
// -------------------------------------------------------------------

#define PWM_MODULE CCU40

#define PWM_SLICE_LO CCU40_CC40
#define PWM_SLICE_LO_NUM 0
#define PWM_SLICE_LO_TRANSFER (XMC_CCU4_SHADOW_TRANSFER_SLICE_0 | XMC_CCU4_SHADOW_TRANSFER_PRESCALER_SLICE_0)
#define PWM_SLICE_LO_PIN P0_15

#define PWM_SLICE_HI CCU40_CC43
#define PWM_SLICE_HI_NUM 3
#define PWM_SLICE_HI_TRANSFER (XMC_CCU4_SHADOW_TRANSFER_SLICE_3 | XMC_CCU4_SHADOW_TRANSFER_PRESCALER_SLICE_3)

#define PWM_PERIOD_LO 35 // 0.25 us
#define PWM_PERIOD_HI 86 // 0.6 us

static const XMC_CCU4_SLICE_COMPARE_CONFIG_t pwm_slice_config =
{
  .timer_mode = (uint32_t) XMC_CCU4_SLICE_TIMER_COUNT_MODE_EA,
  .monoshot = (uint32_t) true,
  .prescaler_mode = (uint32_t) XMC_CCU4_SLICE_PRESCALER_MODE_NORMAL,
  .prescaler_initval = (uint32_t) XMC_CCU4_SLICE_PRESCALER_1
};

static const XMC_CCU4_SLICE_EVENT_CONFIG_t pwm_start_config =
{
  .duration = XMC_CCU4_SLICE_EVENT_FILTER_5_CYCLES,
  .edge     = XMC_CCU4_SLICE_EVENT_EDGE_SENSITIVITY_RISING_EDGE,
  .mapped_input = XMC_CCU4_SLICE_INPUT_D
};

static void pwm_init(void)
{
  // Ensure fCCU reaches CCU42
  XMC_CCU4_SetModuleClock(PWM_MODULE, XMC_CCU4_CLOCK_SCU);

  // Enable clock, enable prescaler block and configure global control
  XMC_CCU4_Init(PWM_MODULE, XMC_CCU4_SLICE_MCMS_ACTION_TRANSFER_PR_CR);

  // Start the prescaler and restore clocks to slices
  XMC_CCU4_StartPrescaler(PWM_MODULE);

  // Ensure fCCU reaches CCU40
  XMC_CCU4_SetModuleClock(PWM_MODULE, XMC_CCU4_CLOCK_SCU);

  // Initialize the Slice
  XMC_CCU4_SLICE_CompareInit(PWM_SLICE_LO, &pwm_slice_config);
  XMC_CCU4_SLICE_CompareInit(PWM_SLICE_HI, &pwm_slice_config);

  // set period and compare
  XMC_CCU4_SLICE_SetTimerPeriodMatch(PWM_SLICE_LO, PWM_PERIOD_LO - 1);
  XMC_CCU4_SLICE_SetTimerPeriodMatch(PWM_SLICE_HI, PWM_PERIOD_HI - 1);
  XMC_CCU4_SLICE_SetTimerCompareMatch(PWM_SLICE_LO, 0);
  XMC_CCU4_SLICE_SetTimerCompareMatch(PWM_SLICE_HI, 0);
  XMC_CCU4_EnableShadowTransfer(PWM_MODULE, PWM_SLICE_HI_TRANSFER | PWM_SLICE_LO_TRANSFER);

  // configure start events
  XMC_CCU4_SLICE_ConfigureEvent(PWM_SLICE_LO, XMC_CCU4_SLICE_EVENT_1, &pwm_start_config);
  XMC_CCU4_SLICE_ConfigureEvent(PWM_SLICE_HI, XMC_CCU4_SLICE_EVENT_1, &pwm_start_config);
  XMC_CCU4_SLICE_StartConfig(PWM_SLICE_LO, XMC_CCU4_SLICE_EVENT_1, XMC_CCU4_SLICE_START_MODE_TIMER_START_CLEAR);
  XMC_CCU4_SLICE_StartConfig(PWM_SLICE_HI, XMC_CCU4_SLICE_EVENT_1, XMC_CCU4_SLICE_START_MODE_TIMER_START_CLEAR);

  // Get the slice out of idle mode
  XMC_CCU4_EnableClock(PWM_MODULE, PWM_SLICE_LO_NUM);
  XMC_CCU4_EnableClock(PWM_MODULE, PWM_SLICE_HI_NUM);

  // initialize GPIOs
  XMC_GPIO_Init(PWM_SLICE_LO_PIN, &gpio_output_alt3_config);
}

// -------------------------------------------------------------------
// --- public API
// -------------------------------------------------------------------

void stripe_init(void)
{
  // initialize subsystems
  spi_init();
  eru_init();
  pwm_init();
}

void stripe_send(const void *src, const uint32_t length)
{
  // clear pending request for transmit DMA Channel
  XMC_DMA_CH_ClearDestinationPeripheralRequest(XMC_DMA0, 0);

  // Hardware trigger for DMA sending
  XMC_USIC_CH_TriggerServiceRequest(XMC_SPI1_CH0, 0);

  XMC_DMA_CH_SetBlockSize(XMC_DMA0, 0, length);
  XMC_DMA_CH_SetSourceAddress(XMC_DMA0, 0, (uint32_t)src);

  // transmit DMA Channel
  XMC_DMA_CH_Enable(XMC_DMA0, 0);
}

