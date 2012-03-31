
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/delay.h>

#include "wm8994_def.h"


/*Audio Routing routines for the universal board..wm8994 codec*/

void wm8994_disable_playback_path(struct snd_soc_codec *codec, int mode)
{
	u16 val;
	val =wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );

	switch(mode)
	{
		case MM_AUDIO_PLAYBACK_RCV:
		printk("..Disabling RCV\n");
		val&= ~(WM8994_HPOUT2_ENA_MASK);
		//disbale the HPOUT2	
		break;

		case MM_AUDIO_PLAYBACK_SPK:
		printk("..Disabling SPK\n");
		//disbale the SPKOUTL
		val&= ~(WM8994_SPKOUTL_ENA_MASK ); 
		break;

		case MM_AUDIO_PLAYBACK_HP:
		printk("..Disabling HP\n");
		//disble the HPOUT1
		val&=~(WM8994_HPOUT1L_ENA_MASK |WM8994_HPOUT1R_ENA_MASK );
		break;

		default:
		break;

	}
		wm8994_write(codec,WM8994_POWER_MANAGEMENT_1 ,val);
}


void wm8994_playback_route_headset(struct snd_soc_codec *codec, int mode)
{
	u16 val;
	
	printk("Playback Path is now Headset\n");
	//wm8994_disable_playback_path(codec,SPK);
	wm8994_disable_playback_path(codec, mode);
	//Enable vmid,bias,hp left and right
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_HPOUT1L_ENA_MASK | WM8994_HPOUT1R_ENA_MASK );
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL  | WM8994_HPOUT1R_ENA   | WM8994_HPOUT1L_ENA  );  
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);
	
	// Enbale DAC1L to HPOUT1L path
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_1 );
	val &=  ~WM8994_DAC1L_TO_HPOUT1L_MASK;
	val |= WM8994_DAC1L_TO_HPOUT1L ;  	
	wm8994_write(codec,WM8994_OUTPUT_MIXER_1 ,val);

	// Enbale DAC1R to HPOUT1R path
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_2 );
	val &= ~WM8994_DAC1R_TO_HPOUT1R_MASK ;
	val |= WM8994_DAC1R_TO_HPOUT1R ;
	wm8994_write(codec,WM8994_OUTPUT_MIXER_2 ,val);
	//Enable Charge Pump	

	val = wm8994_read(codec,WM8994_CHARGE_PUMP_1);
	val &= ~WM8994_CP_ENA_MASK ;
	val = WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT ; // this is from wolfson  	
	wm8994_write(codec,WM8994_CHARGE_PUMP_1 ,val);
	// Intermediate HP settings
	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1); 	
	val &= ~(WM8994_HPOUT1R_DLY_MASK |WM8994_HPOUT1R_OUTP_MASK |WM8994_HPOUT1R_RMV_SHORT_MASK |
		WM8994_HPOUT1L_DLY_MASK |WM8994_HPOUT1L_OUTP_MASK | WM8994_HPOUT1L_RMV_SHORT_MASK );

	val |= (WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP|WM8994_HPOUT1L_DLY |WM8994_HPOUT1R_RMV_SHORT| 
		WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY   );
	wm8994_write(codec, WM8994_ANALOGUE_HP_1 ,val);

	//Configuring the Digital Paths
	//Enable Dac1 and DAC2 and the Timeslot0 for AIF1	
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5 ); 	
	val &= ~(WM8994_DAC1R_ENA_MASK | WM8994_DAC1L_ENA_MASK|WM8994_AIF1DAC1R_ENA_MASK |  WM8994_AIF1DAC1L_ENA_MASK );
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA  | WM8994_DAC1L_ENA |WM8994_DAC1R_ENA );
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5 ,val);

	// Unmute the AF1DAC1	
	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1 ); 	
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK);
	wm8994_write(codec,WM8994_AIF1_DAC1_FILTERS_1 ,val);
	
	// Enable the Timeslot0 to DAC1L
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING  ); 	
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK);
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec,WM8994_DAC1_LEFT_MIXER_ROUTING ,val);
	//Enable the Timeslot0 to DAC1R
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING  ); 	
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK);
	val |= WM8994_AIF1DAC1R_TO_DAC1R;
	wm8994_write(codec,WM8994_DAC1_RIGHT_MIXER_ROUTING ,val);
	//Unmute and volume ctrl LeftDAC
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME ); 
	val &= ~(WM8994_DAC1L_MUTE_MASK);
	val |= 0xc0; //0 db volume 	
	wm8994_write(codec,WM8994_DAC1_LEFT_VOLUME,val);
	//Unmute and volume ctrl RightDAC
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME ); 
	val &= ~(WM8994_DAC1R_MUTE_MASK);
	val |= 0xc0; //0 db volume 	
	wm8994_write(codec,WM8994_DAC1_RIGHT_VOLUME,val);
}

void wm8994_disable_rec_path(struct snd_soc_codec *codec, int mode) 
{
        u16 val;
        val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);

        switch(mode)
        {
                case MM_AUDIO_VOICEMEMO_MAIN:
                //printk("Disabling MAIN Mic Path..\n");
                val&= ~(WM8994_MICB1_ENA_MASK);
                break;
		
                case MM_AUDIO_VOICEMEMO_SUB:
                //printk("Disbaling SUB Mic path..\n");
                val&= ~(WM8994_MICB1_ENA_MASK);
                break;
	
		default:
                break;
        }

        wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);
}

void wm8994_record_headset_mic(struct snd_soc_codec *codec, int mode) 
{
	u16 val;
	//wm8994_disable_rec_path(codec, mode);
	printk("Recording through Headset Mic\n");

	//Enable mic bias, vmid, bias generator
	//val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	//val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_MICB1_ENA_MASK  );
	//val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |WM8994_MICB1_ENA  );  
	//wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	//Enable Right Input Mixer,Enable IN1R PGA
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2 );
	val &= ~(WM8994_IN1R_ENA_MASK |WM8994_MIXINR_ENA_MASK  );
	val |= (WM8994_MIXINR_ENA | WM8994_IN2R_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);
	
	// Enable volume,unmute Right Line	
	val = wm8994_read(codec,WM8994_RIGHT_LINE_INPUT_1_2_VOLUME);	
	val &= ~(WM8994_IN1R_VU_MASK|WM8994_IN1R_MUTE_MASK  );
	val |= WM8994_IN1R_VU | WM8994_IN1R_VOL_30dB ;
	wm8994_write(codec,WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,val);

	//connect in1rn to inr1 and in1rp to inrp
	val = wm8994_read(codec,WM8994_INPUT_MIXER_2);
	val &= ~( WM8994_IN1RN_TO_IN1R_MASK | WM8994_IN1RP_TO_IN1R_MASK);
	val |= (WM8994_IN1RN_TO_IN1R | WM8994_IN1RP_TO_IN1R)  ;	
	wm8994_write(codec,WM8994_INPUT_MIXER_2,val);
	
	// unmute right pga, set volume 
	val = wm8994_read(codec,WM8994_INPUT_MIXER_4 );
	val &= ~(WM8994_IN1R_TO_MIXINR_MASK |WM8994_IN1R_MIXINR_VOL_MASK );	
	val |= (WM8994_IN1R_MIXINR_VOL | WM8994_IN1R_TO_MIXINR| 0x7 ); //volume 6db
	wm8994_write(codec,WM8994_INPUT_MIXER_4 ,val);

//Digital Paths	
	//Enable right ADC and time slot
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_ADCR_ENA_MASK |WM8994_AIF1ADC1R_ENA_MASK );
	val |= (WM8994_AIF1ADC1R_ENA | WM8994_ADCR_ENA  );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);
	
//ADC Right mixer routing
	val = wm8994_read(codec,WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING);
	val &= ~( WM8994_ADC1R_TO_AIF1ADC1R_MASK);
	val |= WM8994_ADC1R_TO_AIF1ADC1R;
	wm8994_write(codec,WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING,val);

}

void wm8994_record_sub_mic(struct snd_soc_codec *codec, int mode) 
{
	u16 val;
	//wm8994_disable_rec_path(codec, mode);
	printk("Recording through Sub Mic\n");
	
#if 1

	wm8994_write(codec, 0x39, 0x006C );
	wm8994_write(codec, 0x01, 0x0003 );
	msleep( 50 );

	//Enable micbias,vmid,mic1
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_MICB1_ENA_MASK  );
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |WM8994_MICB1_ENA  );  
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	msleep(50);
	
	// analog 
	wm8994_write(codec, 0x02, 0x6300 ); // MIXINL_ENA=1, MIXINR_ENA=1, IN2L_ENA=0, IN2R_ENA=0
	
	wm8994_write(codec, 0x03, 0x00F0 ); // MIXOUTLVOL_ENA=1, MIXOUTRVOL_ENA=1, MIXOUTL_ENA=1, MIXOUTR_ENA=1
	wm8994_write(codec, 0x1C, 0x0079 ); // Left Output Volume(1CH): 0079  HPOUT1_VU=0, HPOUT1L_ZC=0, HPOUT1L_MUTE_N=1, HPOUT1L_VOL=11_1001
	wm8994_write(codec, 0x1D, 0x0179 ); // Right Output Volume(1DH): 0179  HPOUT1_VU=1, HPOUT1R_ZC=0, HPOUT1R_MUTE_N=1, HPOUT1R_VOL=11_1001
	wm8994_write(codec, 0x18, 0x018B ); // IN1L mute, +0dB gain
	wm8994_write(codec, 0x1A, 0x018B ); // IN1R mute, +0dB gain
	
	wm8994_write(codec, 0x2B, 0x0007); // Input Mixer (5)(2BH):    0007  VRXN_MIXINL_VOL = 111(6dB) / PGA bypass, 
	wm8994_write(codec, 0x2C, 0x0007); // Input Mixer (5)(2CH):    0007  VRXN_MIXINR_VOL = 111(6dB) / PGA bypass, 	
	
	//wm8994_write(codec, 0x2D, 0x0840 ); // MIXINL to MIXOUTL unmute / 000 ( 0dB)
	//wm8994_write(codec, 0x2E, 0x0840 ); // MIXINR to MIXOUTR unmute / 000 ( 0dB)

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable

	wm8994_write(codec, 0x4C, 0x9F25 );
	msleep( 10 );
	wm8994_write(codec, 0x01, 0x0313 );
	wm8994_write(codec, 0x60, 0x0022 );
	wm8994_write(codec, 0x54, 0x0033 );
	msleep( 200 );
	wm8994_write(codec, 0x60, 0x00EE );

	//3 Digital Paths   ADCL
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_ADCL_ENA_MASK|WM8994_AIF1ADC1L_ENA_MASK );
	val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);

	//3 Enable timeslots
	val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
	val |=WM8994_ADC1L_TO_AIF1ADC1L ;  
	wm8994_write(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING ,val);

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable
	
	wm8994_write(codec, 0x38, 0x0040 ); // HPOUT2_IN_ENA
#else
	//3 Enable micbias,vmid,mic1
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_MICB1_ENA_MASK  );
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |WM8994_MICB1_ENA  );  
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	msleep(50);

	//3 Enable left input mixer and IN2LP/VRXN PGA	
	//3 Enable right input mixer and IN2RP/VRXP PGA 
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2  );
	val &= ~( WM8994_MIXINL_ENA_MASK /*| WM8994_MIXINR_ENA_MASK*/);
	val |= (WM8994_MIXINL_ENA /*| WM8994_MIXINR_ENA */);
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);

	//3 Unmute IN2LP_MIXINL_VOL 
	val = wm8994_read(codec,WM8994_INPUT_MIXER_5 );	
	val &= ~(WM8994_IN2LP_MIXINL_VOL_MASK );
	val |=0x7; //volume ( 111 = 6dB )
	wm8994_write(codec,WM8994_INPUT_MIXER_5 ,val);
	
	//3 Digital Paths   ADCL
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_ADCL_ENA_MASK|WM8994_AIF1ADC1L_ENA_MASK );
	val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);

	//3 Enable timeslots
	val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
	val |=WM8994_ADC1L_TO_AIF1ADC1L ;  
	wm8994_write(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING ,val);

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable
#endif
}

void wm8994_record_main_mic(struct snd_soc_codec *codec, int mode) 
{
	u16 val;
	//wm8994_disable_rec_path(codec, mode);
	printk("Recording through Main Mic\n");
/*
	val = wm8994_read(codec,WM8994_LDO_1);
	val &= ~(WM8994_LDO1_VSEL_MASK);
	wm8994_write(codec,WM8994_LDO_1,val);

	val = wm8994_read(codec,WM8994_MICBIAS);
	val &= ~(WM8994_MICB1_LVL_MASK);
	val |= 0x01;
	wm8994_write(codec,WM8994_MICBIAS,val);
*/



#if 1
#if 1
	wm8994_write(codec, 0x39, 0x006C );
	wm8994_write(codec, 0x01, 0x0003 );
	msleep( 50 );

	// analog 
#if 0 //SINGLE_POSITIVE_MIC
	wm8994_write(codec, 0x02, 0x6350 ); // MIXINL_ENA=1, MIXINR_ENA=1, IN2L_ENA=0, IN2R_ENA=0
	
	wm8994_write(codec, 0x03, 0x00F0 ); // MIXOUTLVOL_ENA=1, MIXOUTRVOL_ENA=1, MIXOUTL_ENA=1, MIXOUTR_ENA=1
	wm8994_write(codec, 0x1C, 0x0079 ); // Left Output Volume(1CH): 0079  HPOUT1_VU=0, HPOUT1L_ZC=0, HPOUT1L_MUTE_N=1, HPOUT1L_VOL=11_1001
	wm8994_write(codec, 0x1D, 0x0179 ); // Right Output Volume(1DH): 0179  HPOUT1_VU=1, HPOUT1R_ZC=0, HPOUT1R_MUTE_N=1, HPOUT1R_VOL=11_1001
	wm8994_write(codec, 0x18, 0x010B ); // IN1L unmute, +0dB gain
	wm8994_write(codec, 0x1A, 0x010B ); // IN1R unmute, +0dB gain
	
	wm8994_write(codec, 0x28, 0x0033 ); // Input Mixer (2)(28H):    0022  IN2LP_TO_IN2L=0, IN2LN_TO_IN2L=0, IN1LP_TO_IN1L=0, IN1LN_TO_IN1L=1, IN2RP_TO_IN2R=0, IN2RN_TO_IN2R=0, IN1RP_TO_IN1R=1, IN1RN_TO_IN1R=1
	wm8994_write(codec, 0x29, 0x0030 ); // Input Mixer (3)(29H):    0027  IN2L_TO_MIXINL=0, IN2L_MIXINL_VOL=0, IN1L_TO_MIXINL=0, IN1L_MIXINL_VOL=1, MIXOUTL_MIXINL_VOL=000 MUTE
	wm8994_write(codec, 0x2A, 0x0030 ); // Input Mixer (4)(2AH):    0027  IN2R_TO_MIXINR=0, IN2R_MIXINR_VOL=0, IN1R_TO_MIXINR=1, IN1R_MIXINR_VOL=1, MIXOUTR_MIXINR_VOL=000 MUTE

	wm8994_write(codec, 0x2B, 0x01C0); // Input Mixer (5)(2BH):    01C0  IN1LP_MIXINL_VOL = 111(6dB) / PGA bypass, 
	
	//wm8994_write(codec, 0x2D, 0x0040 ); // MIXINL to MIXOUTL unmute / 000 ( 0dB)
	//wm8994_write(codec, 0x2E, 0x0040 ); // MIXINR to MIXOUTR unmute / 000 ( 0dB)
#else /* DIFFERENTIAL */
	//Enable left input mixer and IN1L PGA	
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2  );
	val &= ~( WM8994_IN1L_ENA_MASK | WM8994_MIXINL_ENA_MASK );
	val |= (WM8994_MIXINL_ENA |WM8994_IN1L_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);
	
	// Unmute IN1L PGA, update volume
	val = wm8994_read(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME );	
	val &= ~(WM8994_IN1L_MUTE_MASK );
	val |= WM8994_IN1L_VOL_30dB; //volume
	wm8994_write(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME ,val);
	
	//Connect IN1LN and IN1LP to the inputs
	val = wm8994_read(codec,WM8994_INPUT_MIXER_2 );	
	val &= ( WM8994_IN1LN_TO_IN1L_MASK |WM8994_IN1LP_TO_IN1L_MASK );
	val |= ( WM8994_IN1LP_TO_IN1L | WM8994_IN1LN_TO_IN1L );
	wm8994_write(codec,WM8994_INPUT_MIXER_2 ,val);

	//IN1L_TO_MIXINL
	val = wm8994_read(codec,WM8994_INPUT_MIXER_3 );
	val &= (WM8994_IN1L_TO_MIXINL_MASK  | WM8994_IN1L_MIXINL_VOL);
	val |= (WM8994_IN1L_TO_MIXINL |WM8994_IN1L_MIXINL_VOL| 0x5);//0db
	wm8994_write(codec,WM8994_INPUT_MIXER_3 ,val); 
#endif

	//Digital Paths
	//Enable Left ADC and time slot
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_ADCL_ENA_MASK |WM8994_AIF1ADC1L_ENA_MASK  );
	val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);

	
	//ADC Left mixer routing
	val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
	val |=WM8994_ADC1L_TO_AIF1ADC1L ;  
	wm8994_write(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING ,val);

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable

	wm8994_write(codec, 0x4C, 0x9F25 );
	msleep( 10 );
	wm8994_write(codec, 0x01, 0x0313 );
	wm8994_write(codec, 0x60, 0x0022 );
	wm8994_write(codec, 0x54, 0x0033 );
	msleep( 200 );
	wm8994_write(codec, 0x60, 0x00EE );

#else
//	wm8994_write(codec, 0x101, 0x0004 );
	wm8994_write(codec, 0x39, 0x006C );
	wm8994_write(codec, 0x01, 0x0003 );
	msleep( 50 );
//	wm8994_write(codec, 0x700, 0x0112 );
//	wm8994_write(codec, 0x02, 0x6800 );
//	wm8994_write(codec, 0x221, 0x0700 );
//	wm8994_write(codec, 0x224, 0x1140 );
//	wm8994_write(codec, 0x220, 0x0002 );
//	wm8994_write(codec, 0x200, 0x0011 );
//	wm8994_write(codec, 0x208, 0x000A );

	// analog 
	wm8994_write(codec, 0x02, 0x6350 ); // MIXINL_ENA=1, MIXINR_ENA=1, IN2L_ENA=0, IN2R_ENA=0
	
	wm8994_write(codec, 0x03, 0x00F0 ); // MIXOUTLVOL_ENA=1, MIXOUTRVOL_ENA=1, MIXOUTL_ENA=1, MIXOUTR_ENA=1
	wm8994_write(codec, 0x1C, 0x0079 ); // Left Output Volume(1CH): 0079  HPOUT1_VU=0, HPOUT1L_ZC=0, HPOUT1L_MUTE_N=1, HPOUT1L_VOL=11_1001
	wm8994_write(codec, 0x1D, 0x0179 ); // Right Output Volume(1DH): 0179  HPOUT1_VU=1, HPOUT1R_ZC=0, HPOUT1R_MUTE_N=1, HPOUT1R_VOL=11_1001
	wm8994_write(codec, 0x18, 0x0115 ); // IN1L unmute, +15dB gain
	wm8994_write(codec, 0x1A, 0x0115 ); // IN1R unmute, +15dB gain
	
	wm8994_write(codec, 0x28, 0x0033 ); // Input Mixer (2)(28H):    0022  IN2LP_TO_IN2L=0, IN2LN_TO_IN2L=0, IN1LP_TO_IN1L=0, IN1LN_TO_IN1L=1, IN2RP_TO_IN2R=0, IN2RN_TO_IN2R=0, IN1RP_TO_IN1R=1, IN1RN_TO_IN1R=1
	wm8994_write(codec, 0x29, 0x0033 ); // Input Mixer (3)(29H):    0027  IN2L_TO_MIXINL=0, IN2L_MIXINL_VOL=0, IN1L_TO_MIXINL=0, IN1L_MIXINL_VOL=1, MIXOUTL_MIXINL_VOL=111
	wm8994_write(codec, 0x2A, 0x0037 ); // Input Mixer (4)(2AH):    0027  IN2R_TO_MIXINR=0, IN2R_MIXINR_VOL=0, IN1R_TO_MIXINR=1, IN1R_MIXINR_VOL=1, MIXOUTR_MIXINR_VOL=111

	wm8994_write(codec, 0x2B, 0x01C0); // Input Mixer (5)(2BH):    01C0  IN1LP_MIXINL_VOL = 111(6dB) / PGA bypass, 
	
	wm8994_write(codec, 0x2D, 0x0040 ); // MIXINL to MIXOUTL unmute / 000 ( 0dB)
	wm8994_write(codec, 0x2E, 0x0040 ); // MIXINR to MIXOUTR unmute / 000 ( 0dB)

	//Digital Paths
	//Enable Left ADC and time slot
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_ADCL_ENA_MASK |WM8994_AIF1ADC1L_ENA_MASK  );
	val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);

	
	//ADC Left mixer routing
	val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
	val |=WM8994_ADC1L_TO_AIF1ADC1L ;  
	wm8994_write(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING ,val);

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable

	wm8994_write(codec, 0x4C, 0x9F25 );
	msleep( 10 );
	wm8994_write(codec, 0x01, 0x0313 );
	wm8994_write(codec, 0x60, 0x0022 );
	wm8994_write(codec, 0x54, 0x0033 );
	msleep( 200 );
	wm8994_write(codec, 0x60, 0x00EE );
#endif	
#else
	//Enable micbias,vmid,mic1
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_MICB1_ENA_MASK  );
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |WM8994_MICB1_ENA  );  
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	msleep(50);

	//Enable left input mixer and IN1L PGA	
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2  );
	val &= ~( WM8994_IN1L_ENA_MASK | WM8994_MIXINL_ENA_MASK );
	val |= (WM8994_MIXINL_ENA /*|WM8994_IN1L_ENA*/ );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);
#if 0
	// Unmute IN1L PGA, update volume
	val = wm8994_read(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME );	
	val &= ~(WM8994_IN1L_MUTE_MASK );
	val |= WM8994_IN1L_VOL_3dB; //volume
	wm8994_write(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME ,val);
	
	//Connect IN1LN and IN1LP to the inputs
	val = wm8994_read(codec,WM8994_INPUT_MIXER_2 );	
	val &= ( /*WM8994_IN1LN_TO_IN1L_MASK |*/WM8994_IN1LP_TO_IN1L_MASK );
	val |= ( WM8994_IN1LP_TO_IN1L /*| WM8994_IN1LN_TO_IN1L */);
	wm8994_write(codec,WM8994_INPUT_MIXER_2 ,val);


	//Unmute the PGA
	val = wm8994_read(codec,WM8994_INPUT_MIXER_3 );
	val&= ~(WM8994_IN1L_TO_MIXINL_MASK   );
	val |= (WM8994_IN1L_TO_MIXINL |WM8994_IN1L_MIXINL_VOL| 0x5);//0db
	wm8994_write(codec,WM8994_INPUT_MIXER_3 ,val); 
#endif

	// IN1LP_MIXINL_VOL : 111 ( 6dB )
	val = wm8994_read(codec,WM8994_INPUT_MIXER_5 );
	val &= ~WM8994_IN1LP_MIXINL_VOL_MASK;
	val |= (0x111 << WM8994_IN1LP_MIXINL_VOL_SHIFT);

	wm8994_write(codec, WM8994_INPUT_MIXER_5, val); // Input Mixer (5)(2BH):    01C0  IN1LP_MIXINL_VOL = 111(6dB) / PGA bypass, 
	
	//Digital Paths
	//Enable Left ADC and time slot
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_ADCL_ENA_MASK |WM8994_AIF1ADC1L_ENA_MASK  );
	val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4 ,val);

	
	//ADC Left mixer routing
	val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
	val |=WM8994_ADC1L_TO_AIF1ADC1L ;  
	wm8994_write(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING ,val);

	wm8994_write( codec, WM8994_GPIO_1, 0xA101 );   // GPIO1 is Input Enable
#endif
}

void wm8994_playback_route_speaker(struct snd_soc_codec *codec, int mode)
{
	u16 val;

	printk("Playing through speaker\n");
	wm8994_disable_playback_path(codec, mode);

	//Enbale bias,vmid and Left speaker
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK |WM8994_SPKOUTL_ENA_MASK  );
	
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |  WM8994_SPKOUTL_ENA  );  
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_3 );
	val &= ~( WM8994_SPKLVOL_ENA_MASK   );
	val |= WM8994_SPKLVOL_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_3 ,val);
	// Unmute the SPKMIXVOLUME
	val = wm8994_read(codec,WM8994_SPKMIXL_ATTENUATION );
	val = 0;	
	wm8994_write(codec,WM8994_SPKMIXL_ATTENUATION  ,val);

	val = wm8994_read(codec,WM8994_SPKOUT_MIXERS );
	val |= WM8994_SPKMIXL_TO_SPKOUTL ;
	wm8994_write(codec,WM8994_SPKOUT_MIXERS,val );

//Unmute the DAC path
	val = wm8994_read(codec,WM8994_SPEAKER_MIXER );
	val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK );
	val |= WM8994_DAC1L_TO_SPKMIXL ;
	wm8994_write(codec,WM8994_SPEAKER_MIXER ,val);
// Eable DAC1 Left and timeslot left
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_5 );	
	val &= ~( WM8994_DAC1L_ENA_MASK | WM8994_AIF1DAC1L_ENA_MASK  );
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_DAC1L_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_5,val);   
//Unmute
	val = wm8994_read(codec,WM8994_AIF1_DAC1_FILTERS_1 );
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK );
	val |= WM8994_AIF1DAC1_UNMUTE ;
	wm8994_write(codec,WM8994_AIF1_DAC1_FILTERS_1,val );
//enable timeslot0 to left dac
	val = wm8994_read(codec,WM8994_DAC1_LEFT_MIXER_ROUTING );
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK );
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec,WM8994_DAC1_LEFT_MIXER_ROUTING ,val);
//Unmute DAC1 left
	val = wm8994_read(codec,WM8994_DAC1_LEFT_VOLUME );
	val &= ~(WM8994_DAC1L_MUTE_MASK );
	val |= 0xd0; //volume?
	wm8994_write(codec,WM8994_DAC1_LEFT_VOLUME ,val);
		
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_2, 0x6300);
        wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1, 0x0259);
}

void wm8994_playback_route_receiver(struct snd_soc_codec *codec, int mode)
{
	u16 val;
        printk("Playing through Receiver\n");
    
        wm8994_disable_playback_path(codec, mode);

	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
        val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK | WM8994_HPOUT2_ENA_MASK );
        val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_HPOUT2_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);

	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_3);
	val &= ~(WM8994_MIXOUTLVOL_ENA_MASK | WM8994_MIXOUTRVOL_ENA_MASK | WM8994_MIXOUTL_ENA_MASK | WM8994_MIXOUTR_ENA_MASK);
        val |= (WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA | WM8994_MIXOUTRVOL_ENA | WM8994_MIXOUTLVOL_ENA);
        wm8994_write(codec,WM8994_POWER_MANAGEMENT_3,val);

	// MIXOUTR to HPOUT2
	val = wm8994_read(codec, WM8994_HPOUT2_MIXER);
	val &= ~(WM8994_MIXOUTLVOL_TO_HPOUT2_MASK);
	val |= WM8994_MIXOUTLVOL_TO_HPOUT2;
	wm8994_write(codec,WM8994_HPOUT2_MIXER, val);

	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_1);
        val &= ~(WM8994_DAC1L_TO_MIXOUTL_MASK);
        val |= (WM8994_DAC1L_TO_MIXOUTL);
        wm8994_write(codec,WM8994_OUTPUT_MIXER_1,val);

        val = wm8994_read(codec,WM8994_OUTPUT_MIXER_2);
        val &= ~(WM8994_DAC1R_TO_MIXOUTR_MASK);
        val |= (WM8994_DAC1R_TO_MIXOUTR);
        wm8994_write(codec,WM8994_OUTPUT_MIXER_2,val);

	// Eable DAC1 Left and timeslot left
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_5 );	
	val &= ~( WM8994_DAC1L_ENA_MASK | WM8994_AIF1DAC1L_ENA_MASK  );
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_DAC1L_ENA );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_5,val);   
	//Unmute
	val = wm8994_read(codec,WM8994_AIF1_DAC1_FILTERS_1 );
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK );
	val |= WM8994_AIF1DAC1_UNMUTE ;
	wm8994_write(codec,WM8994_AIF1_DAC1_FILTERS_1,val );
	//enable timeslot0 to left dac
	val = wm8994_read(codec,WM8994_DAC1_LEFT_MIXER_ROUTING );
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK );
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec,WM8994_DAC1_LEFT_MIXER_ROUTING ,val);
	//Unmute DAC1 left
	val = wm8994_read(codec,WM8994_DAC1_LEFT_VOLUME );
	val &= ~(WM8994_DAC1L_MUTE_MASK );
	val |= 0xd0; //volume?
	wm8994_write(codec,WM8994_DAC1_LEFT_VOLUME ,val);

	// HPOUT2(unmute), HPOUT2_VOL = 0dB
	val = wm8994_read(codec,WM8994_HPOUT2_VOLUME);
	val = ~(WM8994_HPOUT2_MUTE_MASK | WM8994_HPOUT2_VOL_MASK) ;
	val = ( 0 << WM8994_HPOUT2_VOL_SHIFT); // (0: 0dB, 1:-6dB)
	wm8994_write(codec,WM8994_HPOUT2_VOLUME, val );



}

#if 1 
void wm8994_disable_voicecall_path(struct snd_soc_codec *codec, int mode)
{
	u16 val;
	val =wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );

	switch(mode)
	{
		case MM_AUDIO_VOICECALL_RCV:
		printk("..Disabling VOICE_RCV\n");
		val&= ~(WM8994_HPOUT2_ENA_MASK);
		//disbale the HPOUT2	
		break;

		case MM_AUDIO_VOICECALL_SPK:
		printk("..Disabling VOICE_SPK\n");
		//disbale the SPKOUTL
		val&= ~(WM8994_SPKOUTL_ENA_MASK ); 
		break;

		case MM_AUDIO_VOICECALL_HP:
		printk("..Disabling VOICE_HP\n");
		//disble the HPOUT1
		val&=~(WM8994_HPOUT1L_ENA_MASK |WM8994_HPOUT1R_ENA_MASK );
		break;

		default:
		break;

	}
		wm8994_write(codec,WM8994_POWER_MANAGEMENT_1 ,val);
}


//Voice path setting - CP to WM8994
void wm8994_voice_rcv_path(struct snd_soc_codec *codec, int mode)
{

	u16 val;
	
	printk("%s %s %s\n",__FILE__,__FUNCTION__,__LINE__);
	
    wm8994_disable_voicecall_path(codec,mode);

#if 1 // Digital I/F
		printk("WM8994 CP-RCV OUT !!!!!!! \n");
	//	wm8994_write(codec,WM8994_SOFTWARE_RESET, 0x0000 );
	
		wm8994_write(codec, 0x39, 0x006C );
		msleep( 5 );
		wm8994_write(codec, 0x01, 0x0003 ); // BIAS_ENA, VMID = normal
		msleep( 50 );
		wm8994_write(codec, 0x01, 0x0813 ); // + HPOUT2_ENA[11] = 1(Enable).  MIC_BIAS1_ENA[4]=1(Enable)
		wm8994_write(codec, 0x02, 0x6240 ); // Thermal Sensor Enable, Thermal Shutdown Enable, MIXINL_ENA[9] =1, IN1L_ENA[6] = 1, 
		wm8994_write(codec, 0x18, 0x0117 ); // IN1_VU[8] = 1, IN1L_MUTE[7] = 0(unmute), IN1_VOL[4:0] = 10111(18dB) 
		// IN1LN -> IN1L
		wm8994_write(codec, 0x28, 0x0030 ); // IN1LN_TO_IN1L = 1, IN1LP_TO_IN1L
		wm8994_write(codec, 0x29, 0x0020 ); // IN1L_TO_MIXINL[5] = 1(unmute) , IN1L_MIXINL_VOL[4] = 0(0dB)
		wm8994_write(codec, 0x03, 0x00F0 ); // MIXOUTLVOL_ENA, MIXOUTRVOL_ENA, MIXOUTL_ENA, MIXOUTR_ENA
		// DAC1L -> MIXOUTL
		wm8994_write(codec, 0x2D, 0x0001 ); // DAC1L_TO_HPOUT1L = 0, DAC1L_TO_MIXOUTL =
		// DAC1R -> MIXOUTR
		wm8994_write(codec, 0x2E, 0x0001 ); // DAC1R_TO_HPOUT1R = 0, DAC1R_TO_MIXOUTR = 1, 
		// MIXOUTR -> HPOUT2
		wm8994_write(codec, 0x33, 0x0018 ); // MIXOUTLVOL_TO_HPOUT2 unmute, MIXOUTRVOL_TO_HPOUT2 unmute
		wm8994_write(codec, 0x04, 0x2002 ); // AIF2ADCL_ENA, ADCL_ENA
		wm8994_write(codec, 0x05, 0x3303 ); // AIF2DACL_ENA, AIF2DACR_ENA, AIF1DAC1L_ENA, AIF1DAC1R_ENA, DAC1L_EN, DAC1R_EN

		// DIGITAL input setting
		wm8994_write(codec, 0x420, 0x0000 ); // unmute DAC1L AND DAC1R
		wm8994_write(codec, 0x520, 0x0000 ); // unmute AIF2DAC, 
		wm8994_write(codec, 0x601, 0x0005 ); // AIF2DAC2L_TO DAC1L, AIF1DAC1L_TO_DAC1L
		wm8994_write(codec, 0x602, 0x0005 ); // AIF2DAC2R_TO_DAC1R, AIF1DAC1R_TO_DAC1R
		wm8994_write(codec, 0x603, 0x000C ); // ADC2_DAC2_VOL2[8:5] = -36dB(0000) ADC1_DAC2_VOL[3:0] = 0dB(1100), 
		wm8994_write(codec, 0x604, 0x0010 ); // ADC1_TO_DAC2L[4] = Enable(1)
		wm8994_write(codec, 0x610, 0x00C0 ); // DAC1L[9] = 0(unmute), DAC1L_VOL[7:0] = 0xC0 (0dB)
		wm8994_write(codec, 0x611, 0x01C0 ); // DAC1R[9] = 1(mute), DAC1R_VOL[7:0] = 0xC0 (0dB)
		wm8994_write(codec, 0x612, 0x01C0 ); // DAC2L[9] = 1(mute), DAC2L_VOL[7:0] = 0xC0 (0dB)
		wm8994_write(codec, 0x613, 0x01C0 ); // DAC2R[9] = 1(mute), DAC2R_VOL[7:0] = 0xC0 (0dB)
		wm8994_write(codec, 0x620, 0x0000 );
		wm8994_write(codec, 0x621, 0x01C0 ); // Digital Sidetone HPF, fcut-off = 370Hz
		
		wm8994_write(codec, 0x208, 0x0007 ); // 0x000E -> 0x0007 // DSP1, DSP2 processing Enable, SYSCLK = AIF1CLK = 11.2896 MHz
		wm8994_write(codec, 0x210, 0x0073 ); // AIF1 Sample Rate = 44.1kHz AIF1CLK_RATE=256 => AIF1CLK = 11.2896 MHz
		wm8994_write(codec, 0x211, 0x0003 ); // AIF2 Sample Rate = 8kHz AIF2CLK_RATE=256 => AIF2CLK = 8k*256 = 2.048MHz
		wm8994_write(codec, 0x310, 0x4118 ); // DSP A mode, 16bit, BCLK2 invert
#if 0 // LOOPBACK TEST
		wm8994_write(codec, 0x311, 0x4C01); // AIF2_LOOPBACK
#else // AIF2DACL -> DAC1L, AIF3DACR -> DAC1L 
		wm8994_write(codec, 0x311, 0x0000); // AIF2_LOOPBACK
#endif		

#if 0 // CODEC MASTER
		// DIGITAL Master/Slave Mode
		wm8994_write(codec,0x312, 0x4000);	  // AIF2 CODEC MASTER Mode Enable
		wm8994_write(codec,0x313, 0x0000);	  // AIF2 BCLK2 = AIF2CLK = 2.048MHz
		wm8994_write(codec,0x314, 0x0900);	  // AIF2 ADC LRCLK Enable in Slave Mode AIF2ADC_RATE = 256 = BCLK2 / LRCLK2
		wm8994_write(codec,0x315, 0x0900);	  // AIF2 DAC LRCLK Enable in Slave Mode, AIF2DAC_RATE = 256 = BCLK2 / LRCLK2
#else // CODEC SLAVE
		// DIGITAL Master/Slave Mode
		wm8994_write(codec, 0x312, 0x0000 ); // SLAVE mode
#endif
		// FLL1 = 11.2896 MHz
		wm8994_write(codec, 0x220, 0x0005 ); // FLL1 Enable, FLL1 Fractional Mode
		wm8994_write(codec, 0x221, 0x0700 ); // FLL1_OUT_DIV = 8 = Fvco / Fout
		wm8994_write(codec, 0x222, 0x86C2 ); // FLL1_N = 34498
		wm8994_write(codec, 0x224, 0x0C80 ); // 0x0C88 -> 0x0C80 FLL1_CLK_REV_DIV = 1 = MCLK / FLL1, FLL1 = MCLK1
		wm8994_write(codec, 0x223, 0x00E0 ); // FLL1_N = 7, FLL1_GAIN = X1
		wm8994_write(codec, 0x200, 0x0011 ); // AIF1 Enable, AIF1CLK = FLL1 

		// FLL2 = 2.048MHz , fs = 8kHz
		wm8994_write(codec, 0x240, 0x0005 ); // FLL2 Enable, FLL2 Fractional Mode
		wm8994_write(codec, 0x241, 0x2F00 ); // FLL2_OUTDIV = 23 = Fvco / Fout
		wm8994_write(codec, 0x242, 0x3126 ); // FLL2_K = 12582
		wm8994_write(codec, 0x244, 0x0C80 ); // 0x0C88 -> 0x0C80 FLL2_CLK_REV_DIV = 1 = MCLK / FLL2, FLL2 = MCLK1
		wm8994_write(codec, 0x204, 0x0019 ); // AIF2CLK_SRC = FLL2, AIF2CLK_INV[2] = 0,  AIF2CLK_DIV[1] = 0, AIF2CLK_ENA[0] = 1
		wm8994_write(codec, 0x243, 0x0100 ); // FLL2_N[14:5] = 8, FLL2_GAIN[3:0] = X1

		// GPIO
		wm8994_write(codec, 0x700, 0xA101 );  // GPIO1 is Input Enable
		wm8994_write(codec, 0x701, 0x8100 ); // GPIO 2 - Set to MCLK2 input
		wm8994_write(codec, 0x702, 0x8100 ); // GPIO 3 - Set to BCLK2 input
		wm8994_write(codec, 0x703, 0x8100 ); // GPIO 4 - Set to DACLRCLK2 input
		wm8994_write(codec, 0x704, 0x8100 ); // GPIO 5 - Set to DACDAT2 input
		wm8994_write(codec, 0x705, 0xA101 ); // GPIO 6 is Input Enable
		wm8994_write(codec, 0x706, 0x0100 ); // GPIO 7 - Set to ADCDAT2 output

		// Unmute HPOUT2 ( RCV )
		wm8994_write(codec, 0x1F, 0x0000 );
#else // Analog I/F
    //===============================================
	val = wm8994_read(codec,WM8994_ANTIPOP_1 );
	val &= ~(0x0040);
	
	val |= (0x0040 );  
	
	wm8994_write(codec,WM8994_ANTIPOP_1,val);

    //OK..RX POWERBLOCK(OUT)
    //================================================
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK 
	                             |WM8994_HPOUT2_ENA_MASK);
	
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL
                                 | WM8994_HPOUT2_ENA );  
	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);


    //OK..MIC POWERBLOCK(OUT)
    //================================================    
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_3 );
	val &= ~( WM8994_LINEOUT1N_ENA_MASK |WM8994_LINEOUT1P_ENA_MASK
	             |WM8994_MIXOUTL_ENA_MASK|WM8994_MIXOUTR_ENA_MASK
	             |WM8994_MIXOUTLVOL_ENA_MASK |WM8994_MIXOUTRVOL_ENA_MASK
	          );
	val |= ( WM8994_LINEOUT1N_ENA | WM8994_LINEOUT1P_ENA
	             |WM8994_MIXOUTL_ENA |WM8994_MIXOUTR_ENA
	             |WM8994_MIXOUTLVOL_ENA |WM8994_MIXOUTRVOL_ENA
	          );
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_3 ,val);

    //OK..  MIC POWERBLOCK(IN)
    //================================================
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2 );
	val &= ~( WM8994_IN1L_ENA_MASK );	
	val |= (WM8994_IN1L_ENA );  	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);

    //OK..  MIC VOLUME
    //================================================
	val = wm8994_read(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME );
	val &= ~( WM8994_IN1L_MUTE_MASK );	
	val |= (0x0B );  	
	wm8994_write(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME,val);

    //OK..  MIC VOLUME
    //================================================
	val = wm8994_read(codec,WM8994_LINE_OUTPUTS_VOLUME );
	val &= ~( WM8994_LINEOUT1N_MUTE_MASK | WM8994_LINEOUT1P_MUTE_MASK);	
	val |= (0x0000 );  	
	wm8994_write(codec,WM8994_LINE_OUTPUTS_VOLUME,val);

    //OK..
    //================================================
	val = wm8994_read(codec,WM8994_LINE_MIXER_1 );
	val &= ~( WM8994_IN1L_TO_LINEOUT1P_MASK );	
	val |= (WM8994_IN1L_TO_LINEOUT1P);  	
	wm8994_write(codec,WM8994_LINE_MIXER_1,val);


    //RX
    
    //OK..MIXOUTL
    //================================================
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_1 );
	val &= ~( WM8994_IN2LP_TO_MIXOUTL_MASK );	
	val |= (WM8994_IN2LP_TO_MIXOUTL);  	
	wm8994_write(codec,WM8994_OUTPUT_MIXER_1,val);

    //OK..MIXOUTL VOL
    //================================================
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_3 );
	val |= (0x0000);  	
	wm8994_write(codec,WM8994_OUTPUT_MIXER_3,val);


    //================================================
	val = wm8994_read(codec,WM8994_LEFT_OPGA_VOLUME );
	val &= ~( 0x017F );	
	val |= (0x017F);  	
	wm8994_write(codec,WM8994_LEFT_OPGA_VOLUME,val);


    //OK..MIXOUTR
    //==============================================
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_2 );
	val &= ~( WM8994_IN2RP_TO_MIXOUTR_MASK );
	val |= (WM8994_IN2RP_TO_MIXOUTR);  	
	wm8994_write(codec,WM8994_OUTPUT_MIXER_2,val);

    //OK..MIXOUTR VOL
    //================================================
	val = wm8994_read(codec,WM8994_OUTPUT_MIXER_4 );
	val |= (0x0000);  	
	wm8994_write(codec,WM8994_OUTPUT_MIXER_4,val);


    //================================================
	val = wm8994_read(codec,WM8994_RIGHT_OPGA_VOLUME );
	val &= ~( 0x017F );	
	val |= (0x017F);  	
	wm8994_write(codec,WM8994_RIGHT_OPGA_VOLUME,val);

    //OK..HPOUT2MIX EMA
    //================================================ 
	val = wm8994_read(codec,WM8994_HPOUT2_MIXER );
	val &= ~(WM8994_MIXOUTLVOL_TO_HPOUT2_MASK /*| WM8994_MIXOUTRVOL_TO_HPOUT2_MASK*/ /*| WM8994_IN2LRP_TO_HPOUT2_MASK*/);
	val |= (WM8994_MIXOUTLVOL_TO_HPOUT2 /*| WM8994_MIXOUTRVOL_TO_HPOUT2*//* |WM8994_IN2LRP_TO_HPOUT2*/);  
	wm8994_write(codec,WM8994_HPOUT2_MIXER,val);

    //OK..HPOUT2 VOL
    //================================================
	val = wm8994_read(codec,WM8994_HPOUT2_VOLUME);
	val &= ~(WM8994_HPOUT2_MUTE_MASK);
	val |= (0x0000);  
	wm8994_write(codec,WM8994_HPOUT2_VOLUME,val);

#endif	
}

void wm8994_voice_ear_path(struct snd_soc_codec *codec, int mode)
{
	u16 val;
	
	printk("%s %s %s\n",__FILE__,__FUNCTION__,__LINE__);
	
    //wm8994_disable_voicecall_path(codec,mode);
#if 1
    wm8994_write(codec, 0x1C, 0x012D );
    wm8994_write(codec, 0x1D, 0x012D );
    msleep( 1000 );
    wm8994_write(codec, 0x57, 0x9C9C );
    wm8994_write(codec, 0x55, 0x054E );
    wm8994_write(codec, 0x54, 0x300F );
    wm8994_write(codec, 0x5B, 0x0080 );
    wm8994_write(codec, 0x5C, 0x0080 );
    wm8994_write(codec, 0x39, 0x006C );
    msleep( 5 );
    wm8994_write(codec, 0x01, 0x0003 );
    msleep( 50 );
    wm8994_write(codec, 0x5B, 0x00FF );
    wm8994_write(codec, 0x5C, 0x00FF );
    msleep( 5 );
    wm8994_write(codec, 0x4C, 0x9F25 );
    msleep( 5 );
    wm8994_write(codec, 0x01, 0x0313 );
    msleep( 5 );
    wm8994_write(codec, 0x60, 0x0022 );
    msleep( 300 );
    wm8994_write(codec, 0x54, 0x333F );
    msleep( 5 );
    msleep( 5 );
    wm8994_write(codec, 0x60, 0x00EE );
    wm8994_write(codec, 0x02, 0x6110 );
    wm8994_write(codec, 0x1A, 0x0117 );
    wm8994_write(codec, 0x28, 0x0003 ); // 0x0001 -> 0x0003
    wm8994_write(codec, 0x29, 0x0100 );
    wm8994_write(codec, 0x2A, 0x0020 ); 
    wm8994_write(codec, 0x2D, 0x0100 );
    wm8994_write(codec, 0x2E, 0x0100 );
    wm8994_write(codec, 0x4C, 0x9F25 );
    wm8994_write(codec, 0x60, 0x00EE );
    wm8994_write(codec, 0x04, 0x3003 );
    wm8994_write(codec, 0x05, 0x3303 );
    wm8994_write(codec, 0x420, 0x0000 );
    wm8994_write(codec, 0x520, 0x0000 );
    wm8994_write(codec, 0x601, 0x0005 );
    wm8994_write(codec, 0x602, 0x0005 );
    wm8994_write(codec, 0x603, 0x000C );
    wm8994_write(codec, 0x604, 0x0010 );
    wm8994_write(codec, 0x610, 0x00C0 );
    wm8994_write(codec, 0x611, 0x01C0 );
    wm8994_write(codec, 0x612, 0x01C0 );
    wm8994_write(codec, 0x613, 0x01C0 );
    wm8994_write(codec, 0x620, 0x0000 );
    wm8994_write(codec, 0x621, 0x01C1 );
    wm8994_write(codec, 0x208, 0x000E );
    wm8994_write(codec, 0x210, 0x0073 );
    wm8994_write(codec, 0x211, 0x0003 );
    wm8994_write(codec, 0x310, 0x4118 );

    //wm8994_write(codec, 0x311, 0x4C01); // AIF2_LOOPBACK

#if 0 // MASTER
		wm8994_write(codec,0x312, 0x4000);	  // AIF2 CODEC MASTER Mode Enable
		wm8994_write(codec,0x313, 0x0000);	  // AIF2 BCLK2 = AIF2CLK = 2.048MHz
		wm8994_write(codec,0x314, 0x0900);	  // AIF2 ADC LRCLK Enable in Slave Mode AIF2ADC_RATE = 256 = BCLK2 / LRCLK2
		wm8994_write(codec,0x315, 0x0900);	  // AIF2 DAC LRCLK Enable in Slave Mode, AIF2DAC_RATE = 256 = BCLK2 / LRCLK2
#else
		wm8994_write(codec, 0x312, 0x0000 );
#endif	

	
    wm8994_write(codec, 0x220, 0x0005 );
    wm8994_write(codec, 0x221, 0x0700 );
    wm8994_write(codec, 0x222, 0x86C2 );
    wm8994_write(codec, 0x224, 0x0C80 );
    wm8994_write(codec, 0x223, 0x00E0 );
    wm8994_write(codec, 0x200, 0x0011 );
    wm8994_write(codec, 0x240, 0x0005 );
    wm8994_write(codec, 0x241, 0x2F00 );
    wm8994_write(codec, 0x242, 0x3126 );
    wm8994_write(codec, 0x244, 0x0C80 );
    wm8994_write(codec, 0x204, 0x0019 );
    wm8994_write(codec, 0x243, 0x0100 );
    wm8994_write(codec, 0x700, 0xA101 );
    wm8994_write(codec, 0x701, 0x8100 );
    wm8994_write(codec, 0x702, 0x8100 );
    wm8994_write(codec, 0x703, 0x8100 );
    wm8994_write(codec, 0x704, 0x8100 );
    wm8994_write(codec, 0x705, 0xA101 );
    wm8994_write(codec, 0x706, 0x0100 );
    msleep( 5 );
    wm8994_write(codec, 0x1C, 0x016D );
    wm8994_write(codec, 0x1D, 0x016D );
    msleep( 1000 );
    wm8994_write(codec, 0x57, 0x9C9C );
    wm8994_write(codec, 0x55, 0x054E );
    wm8994_write(codec, 0x54, 0x300F );
    wm8994_write(codec, 0x5B, 0x0080 );
    wm8994_write(codec, 0x5C, 0x0080 );


#else
    //3 wm8994_write( codec, 0x1C, 0x012D );
    val = wm8994_read(codec,WM8994_LEFT_OUTPUT_VOLUME ); 
	val = WM8994_HPOUT1_VU | 0x2D;
	wm8994_write(codec,WM8994_LEFT_OUTPUT_VOLUME,val);
	
    //3 wm8994_write( codec, 0x1D, 0x012D );
    val = wm8994_read(codec,WM8994_RIGHT_OUTPUT_VOLUME ); 
	val = WM8994_HPOUT1_VU | 0x2D;
	wm8994_write(codec,WM8994_RIGHT_OUTPUT_VOLUME,val);
	
    //3 wm8994_write( codec, 0x39, 0x006C );
    val = wm8994_read(codec,WM8994_ANTIPOP_2 ); 
	val = WM8994_VMID_RAMP_MASK | WM8994_VMID_BUF_ENA | WM8994_STARTUP_BIAS_ENA;
	wm8994_write(codec,WM8994_ANTIPOP_2,val);
	
    mdelay( 5 );
    
    //3 wm8994_write( codec, 0x01, 0x0003 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 ); 
	val = WM8994_VMID_SEL_NORMAL | WM8994_BIAS_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);
	
    mdelay( 50 );
    
    //3 wm8994_write( codec, 0x4C, 0x9F25 );
    val = wm8994_read(codec,WM8994_CHARGE_PUMP_1 ); 
	val = WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT;
	wm8994_write(codec,WM8994_CHARGE_PUMP_1,val);
	
    mdelay( 5 );
    
    //3 wm8994_write( codec, 0x01, 0x0B13 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 ); 
	val = WM8994_HPOUT2_ENA | WM8994_HPOUT1L_ENA | WM8994_HPOUT1R_ENA | WM8994_MICB1_ENA | WM8994_VMID_SEL_NORMAL | WM8994_BIAS_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);
	
    mdelay( 5 );
    
    //3 wm8994_write( codec, 0x60, 0x0022 );
    val = wm8994_read(codec,WM8994_ANALOGUE_HP_1 ); 
	val = WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_DLY;
	wm8994_write(codec,WM8994_ANALOGUE_HP_1,val);
	
    mdelay( 300 );
    
    //3 wm8994_write( codec, 0x54, 0x0033 );
    val = wm8994_read(codec,WM8994_DC_SERVO_1 ); 
	val = WM8994_DCS_TRIG_STARTUP_1 | WM8994_DCS_TRIG_STARTUP_0 | WM8994_DCS_ENA_CHAN_1 | WM8994_DCS_ENA_CHAN_0;
	wm8994_write(codec,WM8994_DC_SERVO_1,val);
	
    mdelay( 50 );
    
    mdelay( 5 );
    
    //3 wm8994_write( codec, 0x60, 0x00EE );
    val = wm8994_read(codec,WM8994_ANALOGUE_HP_1 ); 
	val = WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP | WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT | WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY;
	wm8994_write(codec,WM8994_ANALOGUE_HP_1,val);
	
    //3 wm8994_write( codec, 0x221, 0x0700 );
    val = wm8994_read(codec,WM8994_FLL1_CONTROL_2 ); 
	val = WM8994_FLL1_OUTDIV_8;
	wm8994_write(codec,WM8994_FLL1_CONTROL_2,val);
	
    //3 wm8994_write( codec, 0x224, 0x1140 );
    val = wm8994_read(codec,WM8994_FLL1_CONTROL_5 ); 
	val = 0x1140;
	wm8994_write(codec,WM8994_FLL1_CONTROL_5,val);
	
    //3 wm8994_write( codec, 0x220, 0x0002 );
    val = wm8994_read(codec,WM8994_FLL1_CONTROL_1 ); 
	val = WM8994_FLL1_OSC_ENA;
	wm8994_write(codec,WM8994_FLL1_CONTROL_1,val);
	
    //3 wm8994_write( codec, 0x200, 0x0011 );
    val = wm8994_read(codec,WM8994_AIF1_CLOCKING_1 ); 
	val = WM8994_AIF1CLK_SRC_FLL1 | WM8994_AIF1CLK_ENA;
	wm8994_write(codec,WM8994_AIF1_CLOCKING_1,val);
	
    //3 wm8994_write( codec, 0x208, 0x000A );
    val = wm8994_read(codec,WM8994_CLOCKING_1 ); 
	val = WM8994_DSP_FS1CLK_ENA | WM8994_DSP_FSINTCLK_ENA;
	wm8994_write(codec,WM8994_CLOCKING_1,val);
	
    //3 wm8994_write( codec, 0x02, 0x6850 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2 ); 
	val = WM8994_TSHUT_ENA | WM8994_TSHUT_OPDIS | WM8994_OPCLK_ENA | WM8994_IN1L_ENA | WM8994_IN1R_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);
	
    //3 wm8994_write( codec, 0x18, 0x0180 );
    val = wm8994_read(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME ); 
	val = WM8994_IN1L_VU | WM8994_IN1L_MUTE;
	wm8994_write(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME,val);
	
    //3 wm8994_write( codec, 0x1A, 0x0117 );
    val = wm8994_read(codec,WM8994_RIGHT_LINE_INPUT_1_2_VOLUME ); 
	val = WM8994_IN1R_VU | WM8994_IN1R_VOL_7_5dB | WM8994_IN1R_VOL_n6dB;
	wm8994_write(codec,WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,val);
	
    //3 wm8994_write( codec, 0x28, 0x0011 );
    val = wm8994_read(codec,WM8994_INPUT_MIXER_2 ); 
	val = WM8994_IN1LN_TO_IN1L | WM8994_IN1RN_TO_IN1R;
	wm8994_write(codec,WM8994_INPUT_MIXER_2,val);
	
    //3 wm8994_write( codec, 0x29, 0x0020 );
    val = wm8994_read(codec,WM8994_INPUT_MIXER_3 ); 
	val = WM8994_IN1L_TO_MIXINL;
	wm8994_write(codec,WM8994_INPUT_MIXER_3,val);
	
    //3 wm8994_write( codec, 0x2A, 0x0020 );
    val = wm8994_read(codec,WM8994_INPUT_MIXER_4 ); 
	val = WM8994_IN1R_TO_MIXINR;
	wm8994_write(codec,WM8994_INPUT_MIXER_4,val);
	
    //3 wm8994_write( codec, 0x03, 0x33F0 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_3 ); 
	val = WM8994_LINEOUT1N_ENA | WM8994_LINEOUT1P_ENA | WM8994_SPKRVOL_ENA | WM8994_SPKLVOL_ENA | WM8994_MIXOUTLVOL_ENA | WM8994_MIXOUTRVOL_ENA | WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_3,val);
	
    //3 wm8994_write( codec, 0x1E, 0x0006 );
    val = wm8994_read(codec,WM8994_LINE_OUTPUTS_VOLUME ); 
	val = WM8994_LINEOUT2N_MUTE | WM8994_LINEOUT2P_MUTE;
	wm8994_write(codec,WM8994_LINE_OUTPUTS_VOLUME,val);
	
    //3 wm8994_write( codec, 0x2D, 0x0002 );
    val = wm8994_read(codec,WM8994_OUTPUT_MIXER_1 ); 
	val = WM8994_IN2LP_TO_MIXOUTL;
	wm8994_write(codec,WM8994_OUTPUT_MIXER_1,val);
	
    //3 wm8994_write( codec, 0x2E, 0x0002 );
    val = wm8994_read(codec,WM8994_OUTPUT_MIXER_2 ); 
	val = WM8994_IN2RP_TO_MIXOUTR;
	wm8994_write(codec,WM8994_OUTPUT_MIXER_2,val);
	
    //3 wm8994_write( codec, 0x34, 0x0006 );
    val = wm8994_read(codec,WM8994_LINE_MIXER_1 ); 
	val = WM8994_IN1R_TO_LINEOUT1P | WM8994_IN1L_TO_LINEOUT1P;
	wm8994_write(codec,WM8994_LINE_MIXER_1,val);
	
    //3 wm8994_write( codec, 0x4C, 0x9F25 );
    val = wm8994_read(codec,WM8994_CHARGE_PUMP_1 ); 
	val = WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT;
	wm8994_write(codec,WM8994_CHARGE_PUMP_1,val);
	
    //3 wm8994_write( codec, 0x60, 0x00EE );
    val = wm8994_read(codec,WM8994_ANALOGUE_HP_1 ); 
	val = WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP | WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT | WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY;
	wm8994_write(codec,WM8994_ANALOGUE_HP_1,val);

    //3 wm8994_write( codec, 0x1C, 0x006D );
    val = wm8994_read(codec,WM8994_LEFT_OUTPUT_VOLUME ); 
	val = WM8994_HPOUT1L_MUTE_N | 0x002D;
	wm8994_write(codec,WM8994_LEFT_OUTPUT_VOLUME,val);
	
    //3 wm8994_write( codec, 0x1D, 0x016D );
    val = wm8994_read(codec,WM8994_RIGHT_OUTPUT_VOLUME ); 
	val = WM8994_HPOUT1_VU | WM8994_HPOUT1R_MUTE_N | 0x002D;
	wm8994_write(codec,WM8994_RIGHT_OUTPUT_VOLUME,val);
#endif	
}

void wm8994_voice_spkr_path(struct snd_soc_codec *codec, int mode)
{
	u16 val;
	
	printk("%s %s %s\n",__FILE__,__FUNCTION__,__LINE__);
	
    wm8994_disable_voicecall_path(codec,mode);

    //3 wm8994_write(codec, 0x39, 0x006C );
	val = wm8994_read(codec,WM8994_ANTIPOP_2 ); 
	val = WM8994_VMID_RAMP_MASK | WM8994_VMID_BUF_ENA | WM8994_STARTUP_BIAS_ENA;  	
	wm8994_write(codec,WM8994_ANTIPOP_2,val);
	
    mdelay( 5 );

    //3 wm8994_write(codec, 0x01, 0x0003 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 ); 
	val = WM8994_VMID_SEL_NORMAL | WM8994_BIAS_ENA;  	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);
	
    mdelay( 50 );

    //3 wm8994_write(codec, 0x01, 0x3013 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 ); 
	val = WM8994_SPKOUTR_ENA | WM8994_SPKOUTL_ENA | WM8994_MICB1_ENA | WM8994_VMID_SEL_NORMAL | WM8994_BIAS_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);
	
    //3 wm8994_write(codec, 0x02, 0x6240 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_2 ); 
	val = WM8994_TSHUT_ENA | WM8994_TSHUT_OPDIS | WM8994_MIXINL_ENA | WM8994_IN1L_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_2,val);
	
    //3 wm8994_write(codec, 0x18, 0x0117 );
    val = wm8994_read(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME ); 
	val = WM8994_IN1L_VU | WM8994_IN1L_VOL_7_5dB | WM8994_IN1L_VOL_n6dB;
	wm8994_write(codec,WM8994_LEFT_LINE_INPUT_1_2_VOLUME,val);
	
    //3 wm8994_write(codec, 0x28, 0x0010 );
    val = wm8994_read(codec,WM8994_INPUT_MIXER_2 ); 
	val = WM8994_IN1LN_TO_IN1L;
	wm8994_write(codec,WM8994_INPUT_MIXER_2,val);
	
    //3 wm8994_write(codec, 0x29, 0x0020 );
    val = wm8994_read(codec,WM8994_INPUT_MIXER_3 ); 
	val = WM8994_IN1L_TO_MIXINL;
	wm8994_write(codec,WM8994_INPUT_MIXER_3,val);
	
    //3 wm8994_write(codec, 0x03, 0x33F0 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_3 ); 
	val = WM8994_LINEOUT1N_ENA | WM8994_LINEOUT1P_ENA | WM8994_SPKRVOL_ENA | WM8994_SPKLVOL_ENA 
	    | WM8994_MIXOUTLVOL_ENA | WM8994_MIXOUTRVOL_ENA | WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_3,val);
	
    //3 wm8994_write(codec, 0x1E, 0x0006 );
    val = wm8994_read(codec,WM8994_LINE_OUTPUTS_VOLUME ); 
	val = WM8994_LINEOUT2N_MUTE | WM8994_LINEOUT2P_MUTE;
	wm8994_write(codec,WM8994_LINE_OUTPUTS_VOLUME,val);
	
    //3 wm8994_write(codec, 0x34, 0x0006 );
    val = wm8994_read(codec,WM8994_LINE_MIXER_1 ); 
	val = WM8994_IN1R_TO_LINEOUT1P | WM8994_IN1L_TO_LINEOUT1P;
	wm8994_write(codec,WM8994_LINE_MIXER_1,val);
	
    //3 wm8994_write(codec, 0x24, 0x0038 );
    val = wm8994_read(codec,WM8994_SPKOUT_MIXERS ); 
	val = WM8994_IN2LP_TO_SPKOUTL | WM8994_SPKMIXL_TO_SPKOUTL | WM8994_SPKMIXR_TO_SPKOUTL;
	wm8994_write(codec,WM8994_SPKOUT_MIXERS,val);
	
    //3 wm8994_write(codec, 0x22, 0x0000 );
    val = wm8994_read(codec,WM8994_SPKMIXL_ATTENUATION ); 
	val = 0x0000;
	wm8994_write(codec,WM8994_SPKMIXL_ATTENUATION,val);
	
    //3 wm8994_write(codec, 0x22, 0x0100 );
    val = wm8994_read(codec,WM8994_SPKMIXL_ATTENUATION ); 
	val = 0x0100;
	wm8994_write(codec,WM8994_SPKMIXL_ATTENUATION,val);
	
    //3 wm8994_write(codec, 0x23, 0x0103 );
    val = wm8994_read(codec,WM8994_SPKMIXR_ATTENUATION ); 
	val = WM8994_SPKOUT_CLASSAB | WM8994_SPKMIXR_VOL_MASK;
	wm8994_write(codec,WM8994_SPKMIXR_ATTENUATION,val);
	
    //3 wm8994_write(codec, 0x05, 0x0303 );
    val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_5 ); 
	val = WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA | WM8994_DAC1L_ENA | WM8994_DAC1R_ENA;
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_5,val);
	
    //3 wm8994_write(codec, 0x208, 0x000E );
    val = wm8994_read(codec,WM8994_CLOCKING_1 ); 
	val = WM8994_DSP_FS1CLK_ENA | WM8994_DSP_FS2CLK_ENA | WM8994_DSP_FSINTCLK_ENA;
	wm8994_write(codec,WM8994_CLOCKING_1,val);
	
    //3 wm8994_write(codec, 0x1F, 0x0000 );
    val = wm8994_read(codec,WM8994_HPOUT2_VOLUME ); 
	val = 0x0000;
	wm8994_write(codec,WM8994_HPOUT2_VOLUME,val);
}

void wm8994_voice_BT_path(struct snd_soc_codec *codec)
{
	u16 val;

 	printk("%s %s %s\n",__FILE__,__FUNCTION__,__LINE__);
#if 0
    //================================================
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK );
	
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL );  
	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_1,val);


    //================================================
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
	val &= ~(WM8994_AIF2ADCL_ENA_MASK );
	
	val |= (WM8994_AIF2ADCL_ENA_MASK );  
	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_4,val);

    //================================================
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_6 );
	val &= ~(0x0014 );
 	val |= (0x0014);  

//	val &= ~(WM8994_AIF2_ADCDAT_SRC_MASK | 0x0010);
	
//	val |= (WM8994_AIF2_ADCDAT_SRC | 0x0010);  
	
	wm8994_write(codec,WM8994_POWER_MANAGEMENT_6,val);

    //================================================
	val = wm8994_read(codec,WM8994_AIF2_CONTROL_1 );
	val &= ~(0x4188 );
 	val |= (0x4188);  
	
	wm8994_write(codec,WM8994_AIF2_CONTROL_1,val);

    //================================================
	val = wm8994_read(codec,0x312);
	val &= ~(0x0000 );
 	val |= (0x0000);  
	
	wm8994_write(codec,0x312,val);


    //700
	val = wm8994_read(codec,WM8994_GPIO_1 );
	val &= ~(0xA101 );
 	val |= (0xA101);  	
	
	wm8994_write(codec,WM8994_GPIO_1,val);
	
    //702
	val = wm8994_read(codec,WM8994_GPIO_3 ); 
	val &= ~(0x8100 );
 	val |= (0x8100);  	
	wm8994_write(codec,WM8994_GPIO_3,val);
	
    //703
	val = wm8994_read(codec,WM8994_GPIO_4 ); 
	val &= ~(0x8100 );
 	val |= (0x8100);  	
	wm8994_write(codec,WM8994_GPIO_4,val);
	
    //704
	val = wm8994_read(codec,WM8994_GPIO_5 ); 
	val &= ~(0x8100 );
 	val |= (0x8100);  	
	wm8994_write(codec,WM8994_GPIO_5,val);
	
    //705
	val = wm8994_read(codec,WM8994_GPIO_6 ); 
	val &= ~(0xA101 );
 	val |= (0xA101);  	
	wm8994_write(codec,WM8994_GPIO_6,val);

    //706
	val = wm8994_read(codec,WM8994_GPIO_7 ); 
	val &= ~(0x0100 );
 	val |= (0x0100);  	
	wm8994_write(codec,WM8994_GPIO_7,val);
	
    //707
	val = wm8994_read(codec,WM8994_GPIO_8 ); 
	val &= ~(0x8100 );
 	val |= (0x8100);  	
	wm8994_write(codec,WM8994_GPIO_8,val);
	
    //708
	val = wm8994_read(codec,WM8994_GPIO_9 ); 
	val &= ~(0x0100 );
 	val |= (0x0100);  	
	wm8994_write(codec,WM8994_GPIO_9,val);

    //709
	val = wm8994_read(codec,WM8994_GPIO_10 ); 
	val &= ~(0x0100 );
 	val |= (0x0100);  	
	wm8994_write(codec,WM8994_GPIO_10,val);

    //70A
	val = wm8994_read(codec,WM8994_GPIO_11 ); 
	val &= ~(0x0100 );
 	val |= (0x0100);  	
	wm8994_write(codec,WM8994_GPIO_11,val);

#else
    wm8994_write(codec,0x01, 0x0003 );
    wm8994_write(codec,0x04, 0x2000 );
    wm8994_write(codec,0x06, 0x0014 );
    wm8994_write(codec,0x310, 0x4188 );

    wm8994_write(codec,0x312, 0x0000 );

    wm8994_write(codec,0x700, 0xA101 );
    wm8994_write(codec,0x702, 0x8100 );
    wm8994_write(codec,0x703, 0x8100 );
    wm8994_write(codec,0x704, 0x8100 );
    wm8994_write(codec,0x705, 0xA101 );
    wm8994_write(codec,0x706, 0x0100 );
    wm8994_write(codec,0x707, 0x8100 );
    wm8994_write(codec,0x708, 0x0100 );
    wm8994_write(codec,0x709, 0x0100 );
    wm8994_write(codec,0x70A, 0x0100 );
#endif

}
#else
void wm8994_voice_spkr_path(struct snd_soc_codec *codec)
{
	wm8994_write(codec,0x39, 0x006C );                     
	wm8994_write(codec,0x01, 0x0003 );    
	wm8994_write(codec,0x01, 0x3013 );    
	wm8994_write(codec,0x02, 0x6240 );    
	wm8994_write(codec,0x18, 0x011F );    
	wm8994_write(codec,0x28, 0x0010 );    
	wm8994_write(codec,0x29, 0x0020 );    
	wm8994_write(codec,0x03, 0x0300 );    
	wm8994_write(codec,0x22, 0x0000 );    
	wm8994_write(codec,0x23, 0x0100 );    
	wm8994_write(codec,0x25, 0x0168 );    
	wm8994_write(codec,0x36, 0x0003 );    
	wm8994_write(codec,0x04, 0x2002 );    
	wm8994_write(codec,0x05, 0x3303 );    
	wm8994_write(codec,0x520, 0x0000 );   
	wm8994_write(codec,0x601, 0x0005 );   
	wm8994_write(codec,0x602, 0x0005 );   
	wm8994_write(codec,0x603, 0x000C );   
	wm8994_write(codec,0x604, 0x0010 );   
	wm8994_write(codec,0x610, 0x00C0 );   
	wm8994_write(codec,0x611, 0x01C0 );   
	wm8994_write(codec,0x612, 0x01C0 );   
	wm8994_write(codec,0x613, 0x01C0 );   
	wm8994_write(codec,0x620, 0x0000 );   
	wm8994_write(codec,0x621, 0x01C0 );   
	wm8994_write(codec,0x200, 0x0001 );   
	wm8994_write(codec,0x204, 0x0009 );   
	wm8994_write(codec,0x208, 0x000E );   
	wm8994_write(codec,0x210, 0x0073 );   
	wm8994_write(codec,0x211, 0x0003 );   
	wm8994_write(codec,0x312, 0x0000 );   
	wm8994_write(codec,0x310, 0x4110 );   
	wm8994_write(codec,0x220, 0x0005 );   
	wm8994_write(codec,0x221, 0x0700 );   
	wm8994_write(codec,0x222, 0x86C2 );   
	wm8994_write(codec,0x224, 0x0C88 );   
	wm8994_write(codec,0x223, 0x00E0 );   
	wm8994_write(codec,0x200, 0x0011 );   
	wm8994_write(codec,0x240, 0x0005 );   
	wm8994_write(codec,0x241, 0x2F00 );   
	wm8994_write(codec,0x242, 0x3126 );   
	wm8994_write(codec,0x244, 0x0C88 );   
	wm8994_write(codec,0x204, 0x0019 );   
	wm8994_write(codec,0x243, 0x0100 );   
	wm8994_write(codec,0x701, 0x8100 );   
	wm8994_write(codec,0x702, 0x8100 );   
	wm8994_write(codec,0x703, 0x8100 );   
	wm8994_write(codec,0x704, 0x8100 );   
	wm8994_write(codec,0x705, 0xA101 );   
	wm8994_write(codec,0x706, 0x0100 );                      
	wm8994_write(codec,0x22, 0x0000 );    
}
#endif
