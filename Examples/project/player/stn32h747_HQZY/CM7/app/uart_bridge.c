#include "main.h"
#include "usart.h"

#define CHARM_HAL_OK 0
#define CHARM_HAL_ERROR 1
#define CHARM_HAL_BUSY 2
#define CHARM_HAL_TIMEOUT 3
#define CHARM_HAL_UNSUPPORTED 4

struct CharmUartCtx {
    UART_HandleTypeDef* handle;
    IRQn_Type irqn;
    int nvic_ready;
};

static struct CharmUartCtx g_uart1_ctx = { &huart1, USART1_IRQn, 0 };

void* charm_uart1_ctx(void) {
    return &g_uart1_ctx;
}

int charm_uart_init(void* ctx, uint32_t baud, uint8_t data_bits,
                    uint8_t parity, uint8_t stop_bits) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return CHARM_HAL_ERROR;
    }

    UART_HandleTypeDef* h = self->handle;
    h->Instance = USART1;
    h->Init.BaudRate = baud;
    if (data_bits == 8) {
        h->Init.WordLength = UART_WORDLENGTH_8B;
    } else if (data_bits == 9) {
        h->Init.WordLength = UART_WORDLENGTH_9B;
    } else {
        return CHARM_HAL_UNSUPPORTED;
    }

    if (parity == 0) {
        h->Init.Parity = UART_PARITY_NONE;
    } else if (parity == 1) {
        h->Init.Parity = UART_PARITY_EVEN;
    } else if (parity == 2) {
        h->Init.Parity = UART_PARITY_ODD;
    } else {
        return CHARM_HAL_UNSUPPORTED;
    }

    if (stop_bits == 0) {
        h->Init.StopBits = UART_STOPBITS_1;
    } else if (stop_bits == 1) {
        h->Init.StopBits = UART_STOPBITS_2;
    } else {
        return CHARM_HAL_UNSUPPORTED;
    }

    h->Init.Mode = UART_MODE_TX_RX;
    h->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    h->Init.OverSampling = UART_OVERSAMPLING_16;
    h->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    h->Init.ClockPrescaler = UART_PRESCALER_DIV1;
    h->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(h) != HAL_OK) {
        return CHARM_HAL_ERROR;
    }
    if (HAL_UARTEx_SetTxFifoThreshold(h, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        return CHARM_HAL_ERROR;
    }
    if (HAL_UARTEx_SetRxFifoThreshold(h, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        return CHARM_HAL_ERROR;
    }
    if (HAL_UARTEx_DisableFifoMode(h) != HAL_OK) {
        return CHARM_HAL_ERROR;
    }
    return CHARM_HAL_OK;
}

int charm_uart_enable(void* ctx) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return CHARM_HAL_ERROR;
    }
    __HAL_UART_ENABLE(self->handle);
    return CHARM_HAL_OK;
}

int charm_uart_disable(void* ctx) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return CHARM_HAL_ERROR;
    }
    __HAL_UART_DISABLE(self->handle);
    return CHARM_HAL_OK;
}

int charm_uart_try_write(void* ctx, uint8_t byte) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle || !self->handle->Instance) {
        return CHARM_HAL_ERROR;
    }
    USART_TypeDef* inst = self->handle->Instance;
    if ((inst->ISR & USART_ISR_TXE_TXFNF) == 0U) {
        return CHARM_HAL_BUSY;
    }
    inst->TDR = byte;
    return CHARM_HAL_OK;
}

int charm_uart_try_read(void* ctx, uint8_t* byte) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle || !self->handle->Instance || !byte) {
        return CHARM_HAL_ERROR;
    }
    USART_TypeDef* inst = self->handle->Instance;
    uint32_t isr = inst->ISR;
    if (isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) {
        __HAL_UART_CLEAR_FLAG(self->handle,
                              UART_CLEAR_OREF | UART_CLEAR_NEF |
                              UART_CLEAR_FEF | UART_CLEAR_PEF);
        return CHARM_HAL_ERROR;
    }
    if ((isr & USART_ISR_RXNE_RXFNE) == 0U) {
        return CHARM_HAL_BUSY;
    }
    *byte = (uint8_t)(inst->RDR & 0xFFU);
    return CHARM_HAL_OK;
}

void charm_uart_enable_irq(void* ctx, uint32_t mask) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return;
    }
    if (!self->nvic_ready) {
        HAL_NVIC_SetPriority(self->irqn, 5, 0);
        HAL_NVIC_EnableIRQ(self->irqn);
        self->nvic_ready = 1;
    }
    if (mask & (1U << 0)) {
        __HAL_UART_ENABLE_IT(self->handle, UART_IT_RXNE);
    }
    if (mask & (1U << 1)) {
        __HAL_UART_ENABLE_IT(self->handle, UART_IT_TXE);
    }
    if (mask & (1U << 2)) {
        __HAL_UART_ENABLE_IT(self->handle, UART_IT_ERR);
    }
}

void charm_uart_disable_irq(void* ctx, uint32_t mask) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return;
    }
    if (mask & (1U << 0)) {
        __HAL_UART_DISABLE_IT(self->handle, UART_IT_RXNE);
    }
    if (mask & (1U << 1)) {
        __HAL_UART_DISABLE_IT(self->handle, UART_IT_TXE);
    }
    if (mask & (1U << 2)) {
        __HAL_UART_DISABLE_IT(self->handle, UART_IT_ERR);
    }
}

void charm_uart_clear_irq(void* ctx, uint32_t mask) {
    struct CharmUartCtx* self = (struct CharmUartCtx*)ctx;
    if (!self || !self->handle) {
        return;
    }
    if (mask & (1U << 2)) {
        __HAL_UART_CLEAR_FLAG(self->handle,
                              UART_CLEAR_OREF | UART_CLEAR_NEF |
                              UART_CLEAR_FEF | UART_CLEAR_PEF);
    }
}
