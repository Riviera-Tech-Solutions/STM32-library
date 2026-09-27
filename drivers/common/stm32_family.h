#ifndef STM32_FAMILY_H
#define STM32_FAMILY_H
/* A family is a register-layout choice, not an exact-part capability claim. */
#if (defined(STM32F0) + defined(STM32F1) + defined(STM32F2) + \
     defined(STM32F3) + defined(STM32F4) + defined(STM32F7) + \
     defined(STM32G0) + defined(STM32G4) + defined(STM32H5) + \
     defined(STM32H7) + defined(STM32L0) + defined(STM32L1) + \
     defined(STM32L4) + defined(STM32L4PLUS) + defined(STM32L5) + \
     defined(STM32U5) + defined(STM32WB) + defined(STM32WL)) != 1
#error "Define exactly one supported STM32 family (see drivers/README.md)"
#endif
#endif
