/*	EVETR.h
    Copyright (C) 2017  George T. Gougoudis<george.gougoudis@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __EVETR_H__
#define __EVETR_H__

#ifndef CS
#define CS 10
#endif

class EveTransport {
	private:
		byte CSpin;
		byte model;
		SPIClass* mSPI;
	public:
		EveTransport(uint8_t CSpin, SPIClass* mSPI){
			this->CSpin = CSpin;
			this->mSPI = mSPI;
		}
		void ios() {
			pinMode(CSpin, OUTPUT);
			digitalWrite(CSpin, HIGH);
			//pinMode(5, OUTPUT);
			//digitalWrite(5, HIGH);

			mSPI->begin();
			// for (;;) mSPI->transfer(0x33);
		}
		void begin0() {
			ios();

			mSPI->begin();
#ifdef TEENSYDUINO
			mSPI->beginTransaction(SPISettings(3000000, MSBFIRST, SPI_MODE0));
#else
#ifndef __DUE__
			mSPI->setClockDivider(SPI_CLOCK_DIV2);
			SPSR = (1 << SPI2X);
#endif
#endif

			hostcmd(0x00);
#if (BOARD != BOARD_FTDI_81X)
			hostcmd(0x44); // from external crystal
#endif
			hostcmd(0x68);
		}


		void begin1() {
#if 0
			delay(120);
#else
			while ((__rd16(0xc0000UL) & 0xff) != 0x08)
				;
#endif

			// Test point: saturate SPI
			while (0) {
				digitalWrite(CSpin, LOW);
				mSPI->transfer(0x55);
				digitalWrite(CSpin, HIGH);
			}

#if 0
			// Test point: attempt to wake up FT8xx every 2 seconds
			while (0) {
				hostcmd(0x00);
				delay(120);
				hostcmd(0x68);
				delay(120);
				digitalWrite(CSpin, LOW);
				Serial.println(mSPI->transfer(0x10), HEX);
				Serial.println(mSPI->transfer(0x24), HEX);
				Serial.println(mSPI->transfer(0x00), HEX);
				Serial.println(mSPI->transfer(0xff), HEX);
				Serial.println(mSPI->transfer(0x00), HEX);
				Serial.println(mSPI->transfer(0x00), HEX);
				Serial.println();

				digitalWrite(CSpin, HIGH);
				delay(2000);
			}
#endif

			// So that FT800,801      FT81x
			// model       0            1
			ft8xx_model = __rd16(0x0c0000) >> 12;  

			wp = 0;
			freespace = 4096 - 4;

			stream();
		}

		void cmd32(uint32_t x) {
			if (freespace < 4) {
				getfree(4);
			}
			wp += 4;
			freespace -= 4;
			union {
				uint32_t c;
				uint8_t b[4];
			};
			c = x;
			mSPI->transfer(b[0]);
			mSPI->transfer(b[1]);
			mSPI->transfer(b[2]);
			mSPI->transfer(b[3]);
		}
		void cmdbyte(byte x) {
			if (freespace == 0) {
				getfree(1);
			}
			wp++;
			freespace--;
			mSPI->transfer(x);
		}
		void cmd_n(byte *s, uint16_t n) {
			if (freespace < n) {
				getfree(n);
			}
			wp += n;
			freespace -= n;
			while (n > 8) {
				n -= 8;
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
				mSPI->transfer(*s++);
			}
			while (n--)
				mSPI->transfer(*s++);
		}

		void flush() {
			getfree(0);
		}
		uint16_t rp() {
			uint16_t r = __rd16(REG_CMD_READ);
			if (r == 0xfff) {
				//this->eve->alert("COPROCESSOR EXCEPTION");
				Serial.println("COPROCESSOR EXCEPTION");
			}
			return r;
		}
		void finish() {
			wp &= 0xffc;
			__end();
			__wr16(REG_CMD_WRITE, wp);
			while (rp() != wp)
				;
			stream();
		}

		byte rd(uint32_t addr)
		{
			__end(); // stop streaming
			__start(addr);
			mSPI->transfer(0);  // dummy
			byte r = mSPI->transfer(0);
			stream();
			return r;
		}

		void wr(uint32_t addr, byte v)
		{
			__end(); // stop streaming
			__wstart(addr);
			mSPI->transfer(v);
			stream();
		}

		uint16_t rd16(uint32_t addr)
		{
			uint16_t r = 0;
			__end(); // stop streaming
			__start(addr);
			mSPI->transfer(0);
			r = mSPI->transfer(0);
			r |= (mSPI->transfer(0) << 8);
			stream();
			return r;
		}

		void wr16(uint32_t addr, uint32_t v)
		{
			__end(); // stop streaming
			__wstart(addr);
			mSPI->transfer(v);
			mSPI->transfer(v >> 8);
			stream();
		}

		uint32_t rd32(uint32_t addr)
		{
			__end(); // stop streaming
			__start(addr);
			mSPI->transfer(0);
			union {
				uint32_t c;
				uint8_t b[4];
			};
			b[0] = mSPI->transfer(0);
			b[1] = mSPI->transfer(0);
			b[2] = mSPI->transfer(0);
			b[3] = mSPI->transfer(0);
			stream();
			return c;
		}
		void rd_n(byte *dst, uint32_t addr, uint16_t n)
		{
			__end(); // stop streaming
			__start(addr);
			mSPI->transfer(0);
			while (n--)
				*dst++ = mSPI->transfer(0);
			stream();
		}
#ifdef ARDUINO_AVR_UNO
		void wr_n(uint32_t addr, byte *src, uint16_t n)
		{
			__end(); // stop streaming
			__wstart(addr);
			while (n--) {
				SPDR = *src++;
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
			}
			while (!(SPSR & _BV(SPIF))) ;
			stream();
		}
#else
		void wr_n(uint32_t addr, byte *src, uint16_t n)
		{
			__end(); // stop streaming
			__wstart(addr);
			while (n--)
				mSPI->transfer(*src++);
			stream();
		}
#endif

		void wr32(uint32_t addr, unsigned long v)
		{
			__end(); // stop streaming
			__wstart(addr);
			mSPI->transfer(v);
			mSPI->transfer(v >> 8);
			mSPI->transfer(v >> 16);
			mSPI->transfer(v >> 24);
			stream();
		}

		uint32_t getwp(void) {
			return RAM_CMD + (wp & 0xffc);
		}

		void bulk(uint32_t addr) {
			__end(); // stop streaming
			__start(addr);
		}
		void resume(void) {
			stream();
		}

		void __start(uint32_t addr) // start an SPI transaction to addr
		{
			digitalWrite(CSpin, LOW);
			mSPI->transfer(addr >> 16);
			mSPI->transfer(highByte(addr));
			mSPI->transfer(lowByte(addr));  
		}

		void __wstart(uint32_t addr) // start an SPI write transaction to addr
		{
			digitalWrite(CSpin, LOW);
			mSPI->transfer(0x80 | (addr >> 16));
			mSPI->transfer(highByte(addr));
			mSPI->transfer(lowByte(addr));  
		}

		void __end() // end the SPI transaction
		{
			digitalWrite(CSpin, HIGH);
		}

		void stop() // end the SPI transaction
		{
			wp &= 0xffc;
			__end();
			__wr16(REG_CMD_WRITE, wp);
			// while (__rd16(REG_CMD_READ) != wp) ;
		}

		void stream(void) {
			__end();
			__wstart(RAM_CMD + (wp & 0xfff));
		}

		unsigned int __rd16(uint32_t addr)
		{
			unsigned int r;

			__start(addr);
			mSPI->transfer(0);  // dummy
			r = mSPI->transfer(0);
			r |= (mSPI->transfer(0) << 8);
			__end();
			return r;
		}

		void __wr16(uint32_t addr, unsigned int v)
		{
			__wstart(addr);
			mSPI->transfer(lowByte(v));
			mSPI->transfer(highByte(v));
			__end();
		}

		void hostcmd(byte a)
		{
			digitalWrite(CSpin, LOW);
			mSPI->transfer(a);
			mSPI->transfer(0x00);
			mSPI->transfer(0x00);
			digitalWrite(CSpin, HIGH);
		}

		void getfree(uint16_t n)
		{
			wp &= 0xfff;
			__end();
			__wr16(REG_CMD_WRITE, wp & 0xffc);
			do {
				uint16_t fullness = (wp - rp()) & 4095;
				freespace = (4096 - 4) - fullness;
			} while (freespace < n);
			stream();
		}

		byte streaming;
		uint16_t wp;
		uint16_t freespace;
};

#endif
