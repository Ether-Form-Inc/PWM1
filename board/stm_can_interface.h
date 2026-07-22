#pragma once

#include <uavcan/driver/can.hpp>
#include <uavcan/driver/system_clock.hpp>
#include <uavcan/time.hpp>

#include <chrono>

using namespace uavcan;

class Stm_can_iface : public ICanIface
{

  FDCAN_HandleTypeDef hfdcan1;
  bool m_initialized{false};

  FDCAN_TxHeaderTypeDef tx_header;
  FDCAN_RxHeaderTypeDef rx_header;

  public:
  Stm_can_iface()
  {
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = DISABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 4;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 2;
    hfdcan1.Init.NominalTimeSeg2 = 1;
    hfdcan1.Init.DataPrescaler = 1;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 1;
    hfdcan1.Init.DataTimeSeg2 = 1;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE(); // or whatever port your pins are on
    __HAL_RCC_GPIOB_CLK_ENABLE(); // or whatever port your pins are on

    m_initialized = (HAL_FDCAN_Init(&hfdcan1) == HAL_OK);

    HAL_FDCAN_ConfigTimestampCounter(&hfdcan1, FDCAN_TIMESTAMP_PRESC_1);
    HAL_FDCAN_EnableTimestampCounter(&hfdcan1, FDCAN_TIMESTAMP_INTERNAL);

    m_initialized &= (HAL_FDCAN_Start(&hfdcan1) == HAL_OK);

    // FDCAN_FilterTypeDef filter = {0};

    // filter.IdType = FDCAN_EXTENDED_ID;
    // filter.FilterIndex = 0;
    // filter.FilterType = FDCAN_FILTER_MASK;
    // filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    // filter.FilterID1 = 0x00000000;
    // filter.FilterID2 = 0x00000000;

    // HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);

    // tx_header.Identifier = (can_hardware_id << 16);
    tx_header.IdType = FDCAN_EXTENDED_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    // tx_header.DataLength = FDCAN_DLC_BYTES_4;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0x04;
  }
  ~Stm_can_iface() override = default;

  int16_t send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) override
  {
    tx_header.Identifier = frame.id;
    tx_header.DataLength = CanFrame::dlcToDataLength(frame.dlc);
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, frame.data) != HAL_OK)
    {
      return -1;
    }
    return tx_header.DataLength;
  }

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override
  {
    out_flags = 0;

    rx_header = {0};
    memset(out_frame.data, 0, sizeof(out_frame.data));

    uint32_t v{HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0)};
    if (v == 0)
    {
      return 0;
    }

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx_header, out_frame.data) != HAL_OK)
    {
      return 0;
    }

    out_ts_monotonic = uavcan::MonotonicTime::fromUSec(rx_header.RxTimestamp);
    out_ts_utc = uavcan::UtcTime::fromUSec(out_ts_monotonic.toUSec());
    out_frame.dlc = CanFrame::dataLengthToDlc(rx_header.DataLength);
    out_frame.id = rx_header.Identifier;

    out_frame.id |= CanFrame::FlagEFF;
    out_frame.canfd = 0;

    return rx_header.DataLength;
  }
  // TODO: We should probably have a conditional here (or just configure a filter to block non extended

  int16_t configureFilters(const CanFilterConfig *filter_configs, uint16_t num_configs) override
  {
    return -1;
  }

  uint16_t getNumFilters() const override
  {
    return 0;
  }

  uint64_t errors{};
  uint64_t getErrorCount() const override
  {
    return errors;
  }
};

class STM_can_driver : public ICanDriver
{
  Stm_can_iface can_iface;

  public:
  STM_can_driver() = default;
  ~STM_can_driver() override = default;

  ICanIface *getIface(uint8_t iface_index) override
  {
    return &can_iface;
  }

  uint8_t getNumIfaces() const override
  {
    return 1;
  }

  int16_t select(CanSelectMasks &inout_masks, const CanFrame *(&pending_tx)[MaxCanIfaces],
                 MonotonicTime blocking_deadline) override
  {
    return 1;
  }
};

class STM32_system_clock : public uavcan::ISystemClock
{

  mutable uavcan::UtcDuration utc_offset_;

  public:
  STM32_system_clock()
      : utc_offset_(uavcan::UtcDuration::fromUSec(0))
  {
  }

  ~STM32_system_clock() override = default;

  mutable uint32_t count{10};

  uavcan::MonotonicTime getMonotonic() const override
  {
    // return uavcan::MonotonicTime::fromMSec(count++);
    return uavcan::MonotonicTime::fromMSec(HAL_GetTick());
  }

  uavcan::UtcTime getUtc() const override
  {
    return uavcan::UtcTime::fromUSec(getMonotonic().toUSec() + utc_offset_.toUSec());
  }

  void adjustUtc(uavcan::UtcDuration adjustment) override
  {
    utc_offset_ += adjustment;
  }
};
