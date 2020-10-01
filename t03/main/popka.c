void vANInterruptHandler( void )
{
BaseType_t xHigherPriorityTaskWoken;
uint32_t ulStatusRegister;

    /* Read the interrupt status register which has a bit for each interrupt
    source (for example, maybe an Rx bit, a Tx bit, a buffer overrun bit, etc. */
    ulStatusRegister = ulReadPeripheralInterruptStatus();

    /* Clear the interrupts. */
    vClearPeripheralInterruptStatus( ulStatusRegister );

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE.  If calling
    xTaskNotifyFromISR() unblocks the handling task, and the priority of
    the handling task is higher than the priority of the currently running task,
    then xHigherPriorityTaskWoken will automatically get set to pdTRUE. */
    xHigherPriorityTaskWoken = pdFALSE;

    /* Unblock the handling task so the task can perform any processing necessitated
    by the interrupt.  xHandlingTask is the task’s handle, which was obtained
    when the task was created.  The handling task’s 0th notification value
    is bitwise ORed with the interrupt status – ensuring bits that are already
    set are not overwritten. */
    xTaskNotifyIndexedFromISR( xHandlingTask,
                               0,
                               ulStatusRegister,
                               eSetBits,
                               &xHigherPriorityTaskWoken );

    /* Force a context switch if xHigherPriorityTaskWoken is now set to pdTRUE.
    The macro used to do this is dependent on the port and may be called
    portEND_SWITCHING_ISR. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
