module;
#include <cstdint>
#include "fsmc.h"
#include "main.h"
#include "dma.h"
export module lcd_driver;

using u32 = uint32_t;
using u16 = uint16_t;
using vu16 = volatile uint16_t;
using u8 = uint8_t;
#define delay_ms HAL_Delay
inline void delay_us(u32 us)
{
    if (us == 0)
    {
        return;
    }
    const u32 cycles_per_us = SystemCoreClock / 1000000U;
    const u32 cycles = cycles_per_us * us;
    for (volatile u32 i = 0; i < cycles / 5U; ++i)
    {
        __NOP();
    }
}

//LCD重要参数集
typedef struct
{
    u16 LCD_W; //LCD固定宽度(不可修改)
    u16 LCD_H; //LCD固定高度(不可修改)
    u16 width; //LCD实际宽度(根据横竖屏变换)
    u16 height; //LCD实际高度(根据横竖屏变换)
    u16 id; //LCD ID
    u8 dir; //横屏还是竖屏控制：0，竖屏；1，横屏。
    u16 wramcmd; //开始写gram指令
    u16 rramcmd; //开始读gram指令
    u16 setxcmd; //设置x坐标指令
    u16 setycmd; //设置y坐标指令
    u16 setdircmd; //设置显示方向指令
} _lcd_dev;

/////////////////////////////////////用户配置区///////////////////////////////////
#define USE_HORIZONTAL  	  0 //定义液晶屏顺时针旋转方向 	0-0度旋转，1-90度旋转，2-180度旋转，3-270度旋转

////////////////////////////////////////////////////////////////////
//-----------------LCD端口定义----------------
//QDtech全系列模块采用了三极管控制背光亮灭，用户也可以接PWM调节背光亮度

//LCD地址结构体
typedef struct
{
    vu16 LCD_REG;
    vu16 LCD_RAM;
} LCD_TypeDef;

//使用NOR/SRAM的 Bank1.sector1,地址位HADDR[27,26]=00 A16作为数据命令区分线
//使用16位模式时，注意设置时STM32内部会右移一位对齐! 111 1110=0X7E
#define LCD_BASE        ((u32)(0x60000000 | 0x0001FFFE))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

//画笔颜色
#define WHITE       0xFFFF
#define BLACK       0x0000

export void LCD_Init();
export void LCD_Clear(u16 Color);
export void LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd);
export void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
export void LCD_WR_REG(u16 data);
export void LCD_WR_DATA(u16 data);
export void LCD_WriteRAM_Prepare();
export void LCD_direction(u8 direction);
export void LCD_BlitRect565(u16 x, u16 y, u16 w, u16 h, const u16* pixels);

//默认为竖屏
_lcd_dev lcddev;

inline void LCD_Set_BL(bool enable) { HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET); }
inline void LCD_Set_RST(bool enable) { HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET); }

void LCD_WR_REG(vu16 data)
{
    data = data; //使用-O2优化的时候,必须插入的延时
    LCD->LCD_REG = data; //写入要写的寄存器序号
}

void LCD_WR_DATA(vu16 data)
{
    data = data; //使用-O2优化的时候,必须插入的延时
    LCD->LCD_RAM = data; //写入要写的数据
}

u16 LCD_RD_DATA()
{
    vu16 data; //防止被优化
    data = LCD->LCD_RAM;
    return data;
}

void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
{
    LCD->LCD_REG = LCD_Reg; //写入要写的寄存器序号
    LCD->LCD_RAM = LCD_RegValue; //写入数据
}

void LCD_ReadReg(u16 LCD_Reg, u16* Rval, int n)
{
    LCD_WR_REG(LCD_Reg);
    while (n--)
    {
        *(Rval++) = LCD_RD_DATA();
        delay_us(300); //必须加延时
    }
}

void LCD_WriteRAM_Prepare() { LCD_WR_REG(lcddev.wramcmd); }

void LCD_BlitRect565(u16 x, u16 y, u16 w, u16 h, const u16* pixels)
{
    if (!pixels || w == 0 || h == 0) return;
    LCD_SetWindows(x, y, x + w - 1, y + h - 1);
    const u32 count = static_cast<u32>(w) * static_cast<u32>(h);
    for (u32 i = 0; i < count; ++i) {
        LCD->LCD_RAM = pixels[i];
    }
}

void LCD_Clear(u16 Color)
{
    u32 total_point = lcddev.width * lcddev.height;
    if (total_point == 0)
    {
        return;
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);

    u32 remaining = total_point;
    while (remaining > 0)
    {
        u32 chunk = remaining > 65535U ? 65535U : remaining;
        if (HAL_DMA_Start(&hdma_memtomem_dma2_stream0,
                          reinterpret_cast<u32>(&Color),
                          reinterpret_cast<u32>(&LCD->LCD_RAM),
                          chunk) != HAL_OK)
        {
            break;
        }
        if (HAL_DMA_PollForTransfer(&hdma_memtomem_dma2_stream0, HAL_DMA_FULL_TRANSFER, 1000) != HAL_OK)
        {
            break;
        }
        remaining -= chunk;
    }

    if (remaining > 0)
    {
        for (u32 i = 0; i < remaining; ++i)
        {
            LCD->LCD_RAM = Color;
        }
    }
}

void LCD_PWM_BackLightSet(u8 pwm)
{
    LCD_WR_REG(0xBE); //配置PWM输出
    LCD_WR_DATA(0x05); //1设置PWM频率
    LCD_WR_DATA(pwm * 2.55); //2设置PWM占空比
    LCD_WR_DATA(0x01); //3设置C
    LCD_WR_DATA(0xFF); //4设置D
    LCD_WR_DATA(0x00); //5设置E
    LCD_WR_DATA(0x00); //6设置F
}

void LCD_Set_BWTR()
{
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 0); //地址建立时间(ADDSET)清零
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 8); //数据保存时间清零
    if (lcddev.id == 0x5510)
    {
        FSMC_Bank1E->BWTR[0] |= 3 << 0; //地址建立时间(ADDSET)为3个HCLK =18ns
        FSMC_Bank1E->BWTR[0] |= 2 << 8; //数据保存时间(DATAST)为6ns*2个HCLK=12ns
    }
}

void LCD_Init()
{
    MX_DMA_Init();
    MX_FSMC_Init();
    // lcddev.id = LCD_Read_ID();
    lcddev.id = 0x5510;
    LCD_Set_BWTR(); //重新配置写时序控制寄存器的时序使WR时序为最快
    delay_ms(100);
    LCD_Set_RST(false);
    delay_ms(10);
    LCD_Set_RST(true);
    delay_ms(10);
    switch (lcddev.id)
    {
    case 0x5510:
        {
            //************* NT35510初始化**********//
            LCD_WR_REG(0xF000);
            LCD_WR_DATA(0x55);
            LCD_WR_REG(0xF001);
            LCD_WR_DATA(0xAA);
            LCD_WR_REG(0xF002);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xF003);
            LCD_WR_DATA(0x08);
            LCD_WR_REG(0xF004);
            LCD_WR_DATA(0x01);
            //# AVDD: manual); LCD_WR_DATA(
            LCD_WR_REG(0xB600);
            LCD_WR_DATA(0x34);
            LCD_WR_REG(0xB601);
            LCD_WR_DATA(0x34);
            LCD_WR_REG(0xB602);
            LCD_WR_DATA(0x34);
            LCD_WR_REG(0xB000);
            LCD_WR_DATA(0x0D); //09
            LCD_WR_REG(0xB001);
            LCD_WR_DATA(0x0D);
            LCD_WR_REG(0xB002);
            LCD_WR_DATA(0x0D);
            //# AVEE: manual); LCD_WR_DATA( -6V
            LCD_WR_REG(0xB700);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB701);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB702);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB100);
            LCD_WR_DATA(0x0D);
            LCD_WR_REG(0xB101);
            LCD_WR_DATA(0x0D);
            LCD_WR_REG(0xB102);
            LCD_WR_DATA(0x0D);
            //#Power Control for
            //VCL
            LCD_WR_REG(0xB800);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB801);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB802);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB200);
            LCD_WR_DATA(0x00);
            //# VGH: Clamp Enable); LCD_WR_DATA(
            LCD_WR_REG(0xB900);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB901);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB902);
            LCD_WR_DATA(0x24);
            LCD_WR_REG(0xB300);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB301);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB302);
            LCD_WR_DATA(0x05);
            ///LCD_WR_REG(0xBF00); LCD_WR_DATA(0x01);
            //# VGL(LVGL):
            LCD_WR_REG(0xBA00);
            LCD_WR_DATA(0x34);
            LCD_WR_REG(0xBA01);
            LCD_WR_DATA(0x34);
            LCD_WR_REG(0xBA02);
            LCD_WR_DATA(0x34);
            //# VGL_REG(VGLO)
            LCD_WR_REG(0xB500);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xB501);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xB502);
            LCD_WR_DATA(0x0B);
            //# VGMP/VGSP:
            LCD_WR_REG(0xBC00);
            LCD_WR_DATA(0X00);
            LCD_WR_REG(0xBC01);
            LCD_WR_DATA(0xA3);
            LCD_WR_REG(0xBC02);
            LCD_WR_DATA(0X00);
            //# VGMN/VGSN
            LCD_WR_REG(0xBD00);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xBD01);
            LCD_WR_DATA(0xA3);
            LCD_WR_REG(0xBD02);
            LCD_WR_DATA(0x00);
            //# VCOM=-0.1
            LCD_WR_REG(0xBE00);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xBE01);
            LCD_WR_DATA(0x63); //4f
            //  VCOMH+0x01;
            //#R+
            LCD_WR_REG(0xD100);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD101);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD102);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD103);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD104);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD105);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD106);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD107);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD108);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD109);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD10A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD10B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD10C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD10D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD10E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD10F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD110);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD111);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD112);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD113);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD114);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD115);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD116);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD117);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD118);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD119);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD11A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD11B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD11C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD11D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD11E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD11F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD120);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD121);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD122);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD123);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD124);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD125);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD126);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD127);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD128);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD129);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD12A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD12B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD12C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD12D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD12E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD12F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD130);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD131);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD132);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD133);
            LCD_WR_DATA(0xC1);
            //#G+
            LCD_WR_REG(0xD200);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD201);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD202);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD203);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD204);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD205);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD206);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD207);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD208);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD209);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD20A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD20B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD20C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD20D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD20E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD20F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD210);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD211);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD212);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD213);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD214);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD215);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD216);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD217);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD218);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD219);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD21A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD21B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD21C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD21D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD21E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD21F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD220);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD221);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD222);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD223);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD224);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD225);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD226);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD227);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD228);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD229);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD22A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD22B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD22C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD22D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD22E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD22F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD230);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD231);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD232);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD233);
            LCD_WR_DATA(0xC1);
            //#B+
            LCD_WR_REG(0xD300);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD301);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD302);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD303);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD304);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD305);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD306);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD307);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD308);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD309);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD30A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD30B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD30C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD30D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD30E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD30F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD310);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD311);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD312);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD313);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD314);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD315);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD316);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD317);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD318);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD319);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD31A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD31B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD31C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD31D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD31E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD31F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD320);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD321);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD322);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD323);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD324);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD325);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD326);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD327);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD328);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD329);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD32A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD32B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD32C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD32D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD32E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD32F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD330);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD331);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD332);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD333);
            LCD_WR_DATA(0xC1);
            //#R-///////////////////////////////////////////
            LCD_WR_REG(0xD400);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD401);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD402);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD403);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD404);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD405);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD406);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD407);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD408);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD409);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD40A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD40B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD40C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD40D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD40E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD40F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD410);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD411);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD412);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD413);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD414);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD415);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD416);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD417);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD418);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD419);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD41A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD41B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD41C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD41D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD41E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD41F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD420);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD421);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD422);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD423);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD424);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD425);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD426);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD427);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD428);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD429);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD42A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD42B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD42C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD42D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD42E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD42F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD430);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD431);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD432);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD433);
            LCD_WR_DATA(0xC1);
            //#G-//////////////////////////////////////////////
            LCD_WR_REG(0xD500);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD501);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD502);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD503);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD504);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD505);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD506);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD507);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD508);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD509);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD50A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD50B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD50C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD50D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD50E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD50F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD510);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD511);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD512);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD513);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD514);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD515);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD516);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD517);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD518);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD519);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD51A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD51B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD51C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD51D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD51E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD51F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD520);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD521);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD522);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD523);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD524);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD525);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD526);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD527);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD528);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD529);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD52A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD52B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD52C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD52D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD52E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD52F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD530);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD531);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD532);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD533);
            LCD_WR_DATA(0xC1);
            //#B-///////////////////////////////
            LCD_WR_REG(0xD600);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD601);
            LCD_WR_DATA(0x37);
            LCD_WR_REG(0xD602);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD603);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xD604);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD605);
            LCD_WR_DATA(0x7B);
            LCD_WR_REG(0xD606);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD607);
            LCD_WR_DATA(0x99);
            LCD_WR_REG(0xD608);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD609);
            LCD_WR_DATA(0xB1);
            LCD_WR_REG(0xD60A);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD60B);
            LCD_WR_DATA(0xD2);
            LCD_WR_REG(0xD60C);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xD60D);
            LCD_WR_DATA(0xF6);
            LCD_WR_REG(0xD60E);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD60F);
            LCD_WR_DATA(0x27);
            LCD_WR_REG(0xD610);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD611);
            LCD_WR_DATA(0x4E);
            LCD_WR_REG(0xD612);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD613);
            LCD_WR_DATA(0x8C);
            LCD_WR_REG(0xD614);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xD615);
            LCD_WR_DATA(0xBE);
            LCD_WR_REG(0xD616);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD617);
            LCD_WR_DATA(0x0B);
            LCD_WR_REG(0xD618);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD619);
            LCD_WR_DATA(0x48);
            LCD_WR_REG(0xD61A);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD61B);
            LCD_WR_DATA(0x4A);
            LCD_WR_REG(0xD61C);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD61D);
            LCD_WR_DATA(0x7E);
            LCD_WR_REG(0xD61E);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD61F);
            LCD_WR_DATA(0xBC);
            LCD_WR_REG(0xD620);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xD621);
            LCD_WR_DATA(0xE1);
            LCD_WR_REG(0xD622);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD623);
            LCD_WR_DATA(0x10);
            LCD_WR_REG(0xD624);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD625);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xD626);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD627);
            LCD_WR_DATA(0x5A);
            LCD_WR_REG(0xD628);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD629);
            LCD_WR_DATA(0x73);
            LCD_WR_REG(0xD62A);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD62B);
            LCD_WR_DATA(0x94);
            LCD_WR_REG(0xD62C);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD62D);
            LCD_WR_DATA(0x9F);
            LCD_WR_REG(0xD62E);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD62F);
            LCD_WR_DATA(0xB3);
            LCD_WR_REG(0xD630);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD631);
            LCD_WR_DATA(0xB9);
            LCD_WR_REG(0xD632);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xD633);
            LCD_WR_DATA(0xC1);
            //#Enable Page0
            LCD_WR_REG(0xF000);
            LCD_WR_DATA(0x55);
            LCD_WR_REG(0xF001);
            LCD_WR_DATA(0xAA);
            LCD_WR_REG(0xF002);
            LCD_WR_DATA(0x52);
            LCD_WR_REG(0xF003);
            LCD_WR_DATA(0x08);
            LCD_WR_REG(0xF004);
            LCD_WR_DATA(0x00);
            //# RGB I/F Setting
            LCD_WR_REG(0xB000);
            LCD_WR_DATA(0x08);
            LCD_WR_REG(0xB001);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB002);
            LCD_WR_DATA(0x02);
            LCD_WR_REG(0xB003);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB004);
            LCD_WR_DATA(0x02);
            //## SDT:
            LCD_WR_REG(0xB600);
            LCD_WR_DATA(0x08);
            LCD_WR_REG(0xB500);
            LCD_WR_DATA(0x50); //0x6b ???? 480x854       0x50 ???? 480x800
            //## Gate EQ:
            LCD_WR_REG(0xB700);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xB701);
            LCD_WR_DATA(0x00);
            //## Source EQ:
            LCD_WR_REG(0xB800);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xB801);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB802);
            LCD_WR_DATA(0x05);
            LCD_WR_REG(0xB803);
            LCD_WR_DATA(0x05);
            //# Inversion: Column inversion (NVT)
            LCD_WR_REG(0xBC00);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xBC01);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xBC02);
            LCD_WR_DATA(0x00);
            //# BOE's Setting(default)
            LCD_WR_REG(0xCC00);
            LCD_WR_DATA(0x03);
            LCD_WR_REG(0xCC01);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xCC02);
            LCD_WR_DATA(0x00);
            //# Display Timing:
            LCD_WR_REG(0xBD00);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xBD01);
            LCD_WR_DATA(0x84);
            LCD_WR_REG(0xBD02);
            LCD_WR_DATA(0x07);
            LCD_WR_REG(0xBD03);
            LCD_WR_DATA(0x31);
            LCD_WR_REG(0xBD04);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0xBA00);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0xFF00);
            LCD_WR_DATA(0xAA);
            LCD_WR_REG(0xFF01);
            LCD_WR_DATA(0x55);
            LCD_WR_REG(0xFF02);
            LCD_WR_DATA(0x25);
            LCD_WR_REG(0xFF03);
            LCD_WR_DATA(0x01);
            LCD_WR_REG(0x3500);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0x3600);
            LCD_WR_DATA(0x00);
            LCD_WR_REG(0x3a00);
            LCD_WR_DATA(0x55); ////55=16?/////66=18?
            LCD_WR_REG(0x1100);
            delay_ms(120);
            LCD_WR_REG(0x2900);
            LCD_WR_REG(0x2c00);
            lcddev.LCD_W = 480;
            lcddev.LCD_H = 800;
            break;
        }
        default:
            break;
    }
    LCD_direction(USE_HORIZONTAL); //设置LCD显示方向
    LCD_Set_BL(true);
    LCD_Clear(WHITE); //清全屏白色
}

void LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd)
{
    if (lcddev.id == 0x5510)
    {
        LCD_WR_REG(lcddev.setxcmd);
        LCD_WR_DATA(xStar >> 8);
        LCD_WR_REG(lcddev.setxcmd + 1);
        LCD_WR_DATA(xStar & 0XFF);
        LCD_WR_REG(lcddev.setxcmd + 2);
        LCD_WR_DATA(xEnd >> 8);
        LCD_WR_REG(lcddev.setxcmd + 3);
        LCD_WR_DATA(xEnd & 0XFF);
        LCD_WR_REG(lcddev.setycmd);
        LCD_WR_DATA(yStar >> 8);
        LCD_WR_REG(lcddev.setycmd + 1);
        LCD_WR_DATA(yStar & 0XFF);
        LCD_WR_REG(lcddev.setycmd + 2);
        LCD_WR_DATA(yEnd >> 8);
        LCD_WR_REG(lcddev.setycmd + 3);
        LCD_WR_DATA(yEnd & 0XFF);
        LCD_WriteRAM_Prepare(); //开始写入GRAM
        return;
    }
    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(xStar >> 8);
    LCD_WR_DATA(0x00FF & xStar);
    LCD_WR_DATA(xEnd >> 8);
    LCD_WR_DATA(0x00FF & xEnd);

    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(yStar >> 8);
    LCD_WR_DATA(0x00FF & yStar);
    LCD_WR_DATA(yEnd >> 8);
    LCD_WR_DATA(0x00FF & yEnd);

    LCD_WriteRAM_Prepare(); //开始写入GRAM
}

void LCD_direction(u8 direction)
{
    u16 dir_value[4] = {0};
    lcddev.dir = direction % 4;
    if (lcddev.dir % 2) //横屏宽度和高度互换
    {
        lcddev.width = lcddev.LCD_H;
        lcddev.height = lcddev.LCD_W;
    }
    else
    {
        lcddev.width = lcddev.LCD_W;
        lcddev.height = lcddev.LCD_H;
    }
    if (lcddev.id == 0x5510)
    {
        lcddev.setxcmd = 0x2A00;
        lcddev.setycmd = 0x2B00;
        lcddev.wramcmd = 0x2C00;
        lcddev.rramcmd = 0x2E00;
        lcddev.setdircmd = 0x3600;
        dir_value[0] = 0x00;
        dir_value[1] = (1 << 5) | (1 << 6);
        dir_value[2] = (1 << 7) | (1 << 6);
        dir_value[3] = (1 << 7) | (1 << 5);
    }
    switch (lcddev.dir)
    {
    case 0:
        LCD_WriteReg(lcddev.setdircmd, dir_value[0]);
        break;
    case 1:
        LCD_WriteReg(lcddev.setdircmd, dir_value[1]);
        break;
    case 2:
        LCD_WriteReg(lcddev.setdircmd, dir_value[2]);
        break;
    case 3:
        LCD_WriteReg(lcddev.setdircmd, dir_value[3]);
        break;
    default: break;
    }
}
