/*
 * MRF24WG Initialization
 *
 * Functions pertaining Universal Driver and MRF24WG initialization.
 */
#include "wf_universal_driver.h"
#include "wf_global_includes.h"

#define WF_INT_DISABLE ((u_int8_t)0)
#define WF_INT_ENABLE  ((u_int8_t)1)

extern void WF_SetTxDataConfirm(u_int8_t state);

/*
 * Initialize the 16-bit Host Interrupt register on the WiFi device with the
 * specified mask value either setting or clearing the mask register as
 * determined by the input parameter state.
 *
 * Parameters:
 *  hostIntrMaskRegMask - The bit mask to be modified.
 *  state               - One of WF_INT_DISABLE, WF_INT_ENABLE where
 *                        Disable implies clearing the bits and enable sets
 *                        the bits.
 */
static void HostInterrupt2RegInit(u_int16_t hostIntMaskRegMask, u_int8_t  state)
{
    u_int16_t int2MaskValue;

    /* Host Int Register is a status register where each bit indicates a specific event  */
    /* has occurred. In addition, writing a 1 to a bit location in this register clears  */
    /* the event.                                                                        */

    /* Host Int Mask Register is used to enable those events activated in Host Int Register */
    /* to cause an interrupt to the host                                                    */

    /* read current state of int2 mask reg */
    int2MaskValue = WF_Read(WF_HOST_INTR2_MASK_REG);

    /* if caller is disabling a set of interrupts */
    if (state == WF_INT_DISABLE)
    {
        /* zero out that set of interrupts in the interrupt mask copy */
        int2MaskValue &= ~hostIntMaskRegMask;
    }
    /* else caller is enabling a set of interrupts */
    else
    {
        /* set to 1 that set of interrupts in the interrupt mask copy */
        int2MaskValue |= hostIntMaskRegMask;
    }

    /* write out new interrupt mask value */
    WF_Write(WF_HOST_INTR2_MASK_REG, int2MaskValue);

    /* ensure that pending interrupts from those updated interrupts are cleared */
    WF_Write(WF_HOST_INTR2_REG, hostIntMaskRegMask);
}

/*
 * Initialize the 8-bit Host Interrupt register on the MRF24W with the
 * specified mask value either setting or clearing the mask register
 * as determined by the input parameter state.  The process requires
 * 2 spi operations which are performed in a blocking fashion.  The
 * function does not return until both spi operations have completed.
 *
 * Parameters:
 *  hostIntrMaskRegMask - The bit mask to be modified.
 *  state -  one of WF_EXINT_DISABLE, WF_EXINT_ENABLE where
 *           Disable implies clearing the bits and enable sets the bits.
 */
static void HostInterruptRegInit(u_int8_t hostIntrMaskRegMask, u_int8_t state)
{
    u_int8_t hostIntMaskValue;

    /* Host Int Register is a status register where each bit indicates a specific event  */
    /* has occurred. In addition, writing a 1 to a bit location in this register clears  */
    /* the event.                                                                        */

    /* Host Int Mask Register is used to enable those events activated in Host Int Register */
    /* to cause an interrupt to the host                                                    */

    /* read current state of Host Interrupt Mask register */
    hostIntMaskValue = WF_ReadByte(WF_HOST_MASK_REG);

    /* if caller is disabling a set of interrupts */
    if (state == WF_INT_DISABLE)
    {
        /* zero out that set of interrupts in the interrupt mask copy */
        hostIntMaskValue = (hostIntMaskValue & ~hostIntrMaskRegMask);
    }
    /* else caller is enabling a set of interrupts */
    else
    {
        /* set to 1 that set of interrupts in the interrupt mask copy */
        hostIntMaskValue = (hostIntMaskValue & ~hostIntrMaskRegMask) | hostIntrMaskRegMask;
    }

    /* write out new interrupt mask value */
    WF_WriteByte(WF_HOST_MASK_REG, hostIntMaskValue);

    /* ensure that pending interrupts from those updated interrupts are cleared */
    WF_WriteByte(WF_HOST_INTR_REG, hostIntrMaskRegMask);
}

/*
 * Initialize interrupts generated by MRF24WG module.
 */
static void Init_Interrupts()
{
    u_int8_t  mask8;
    u_int16_t mask16;

    // disable the interrupts gated by the 16-bit host int register
    HostInterrupt2RegInit(WF_HOST_2_INT_MASK_ALL_INT, (u_int16_t)WF_INT_DISABLE);

    // disable the interrupts gated the by main 8-bit host int register
    HostInterruptRegInit(WF_HOST_INT_MASK_ALL_INT, WF_INT_DISABLE);

    // Initialize the External Interrupt for the MRF24W allowing the MRF24W to interrupt
    // the Host from this point forward.
    WF_EintInit();
    WF_EintEnable();

    // enable the following MRF24W interrupts in the INT1 8-bit register
    mask8 = (WF_HOST_INT_MASK_FIFO_1_THRESHOLD |     // Mgmt Rx Msg interrupt
             WF_HOST_INT_MASK_FIFO_0_THRESHOLD |     // Data Rx Msg interrupt
             WF_HOST_INT_MASK_RAW_0_INT_0      |     // RAW0 Move Complete (Data Rx) interrupt
             WF_HOST_INT_MASK_RAW_1_INT_0      |     // RAW1 Move Complete (Data Tx) interrupt
             WF_HOST_INT_MASK_INT2);                 // Interrupt 2 interrupt
    HostInterruptRegInit(mask8, WF_INT_ENABLE);

    // enable the following MRF24W interrupts in the INT2 16-bit register
    mask16 = (WF_HOST_INT_MASK_RAW_2_INT_0     |    // RAW2 Move Complete (Mgmt Rx) interrupt
              WF_HOST_INT_MASK_RAW_3_INT_0     |    // RAW3 Move Complete (Mgmt Tx) interrupt
              WF_HOST_INT_MASK_RAW_4_INT_0     |    // RAW4 Move Complete (Scratch) interrupt
              WF_HOST_INT_MASK_RAW_5_INT_0     |    // RAW5 Move Complete (Scratch) interrupt
              WF_HOST_INT_MASK_MAIL_BOX_0_WRT);     // MRF24WG assertion interrupt
    HostInterrupt2RegInit(mask16, WF_INT_ENABLE);
}

/*
 * Initialize the MRF24WG for operations.
 * Must be called before any other WiFi API calls.
 */
void WF_Init(t_deviceInfo *deviceInfo)
{
    unsigned msec, value;

    UdStateInit();          // initialize internal state machine
    EventQInit();           // initialize WiFi event queue
    ClearMgmtConfirmMsg();  // no mgmt response messages received
    UdSetInitInvalid();     // init not valid until it gets through this state machine

    /*
     * Reset the chip.
     */

    // clear the power bit to disable low power mode on the MRF24W
    WF_Write(WF_PSPOLL_H_REG, 0x0000);

    // Set HOST_RESET bit in register to put device in reset
    WF_Write(WF_HOST_RESET_REG, WF_Read(WF_HOST_RESET_REG) | WF_HOST_RESET_MASK);

    // Clear HOST_RESET bit in register to take device out of reset
    WF_Write(WF_HOST_RESET_REG, WF_Read(WF_HOST_RESET_REG) & ~WF_HOST_RESET_MASK);

    /*
     * Wait for Reset to complete, up to 1 sec.
     */
    for (msec=0; msec<1000; msec++) {
        WF_Write(WF_INDEX_ADDR_REG, WF_HW_STATUS_REG);
        value = WF_Read(WF_INDEX_DATA_REG);
        if (value & WF_HW_STATUS_NOT_IN_RESET_MASK)
            break;
        udelay(1000);
    }
printf("--- WF_Init: reset done in %u msec\n", msec);

    /*
     * Wait for chip to initialize itself, up to 2 sec.
     */
    for (msec=0; msec<2000; msec++) {
        value = WF_Read(WF_HOST_WFIFO_BCNT0_REG);
        if (value & 0x0fff)
            break;
        udelay(1000);
    }
printf("--- WF_Init: initialization complete in %u msec\n", msec);

    /*
     * Finish the MRF24WG intitialization.
     */
    Init_Interrupts();                  // Initialize MRF24WG interrupts
    RawInit();                          // initialize RAW driver
    WFEnableMRF24WB0MMode();            // legacy, but still needed
    WF_DeviceInfoGet(deviceInfo);       // get MRF24WG module version numbers
    switch (deviceInfo->deviceType) {
    case WF_UNKNOWN_DEVICE:
        /* Cannot happen. */
        break;

    case WF_MRF24WB_DEVICE:
        /* MRF24WB chip not supported by this driver. */
        break;

    case WF_MRF24WG_DEVICE:
        WF_SetTxDataConfirm(WF_DISABLED); // Disable Tx Data confirms (from the MRF24W)
        WF_CPCreate();                  // create a connection profile, get its ID and store it
        WF_PsPollDisable();
        ClearPsPollReactivate();
        UdSetInitValid();               // Chip initialized successfully.
        break;
    }
}
