/* Copyright (C) Teemu Suutari */

#include "CRMDecompressor.hpp"
#include "HuffmanDecoder.hpp"
#include "DLTADecode.hpp"

bool CRMDecompressor::detectHeader(uint32_t hdr) noexcept
{
	switch (hdr)
	{
		case FourCC('CrM!'):
		case FourCC('CrM2'):
		case FourCC('Crm!'):
		case FourCC('Crm2'):
		return true;

		default:
		return false;
	}
}

bool CRMDecompressor::detectHeaderXPK(uint32_t hdr) noexcept
{
	return hdr==FourCC('CRM2') || hdr==FourCC('CRMS');
}

std::unique_ptr<Decompressor> CRMDecompressor::create(const Buffer &packedData,bool exactSizeKnown,bool verify)
{
	return std::make_unique<CRMDecompressor>(packedData,0,verify);
}

std::unique_ptr<XPKDecompressor> CRMDecompressor::create(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify)
{
	return std::make_unique<CRMDecompressor>(hdr,recursionLevel,packedData,state,verify);
}

CRMDecompressor::CRMDecompressor(const Buffer &packedData,uint32_t recursionLevel,bool verify) :
	XPKDecompressor(recursionLevel),
	_packedData(packedData)
{
	uint32_t hdr=packedData.readBE32(0);
	if (!detectHeader(hdr) || packedData.size()<20)
		throw Decompressor::InvalidFormatError();

	_rawSize=packedData.readBE32(6);
	_packedSize=packedData.readBE32(10);
	if (!_rawSize || !_packedSize ||
		_rawSize>getMaxRawSize() || _packedSize>getMaxPackedSize() ||
		_packedSize+14>packedData.size()) throw Decompressor::InvalidFormatError();
	if (((hdr>>8)&0xff)=='m') _isSampled=true;
	if ((hdr&0xff)=='2') _isLZH=true;
}

CRMDecompressor::CRMDecompressor(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify) :
	CRMDecompressor(packedData,recursionLevel,verify)
{
	_isXPKDelta=(hdr==FourCC('CRMS'));
}

CRMDecompressor::~CRMDecompressor()
{
	// nothing needed
}

const std::string &CRMDecompressor::getName() const noexcept
{
	static std::string names[4]={
		"CrM!: Crunch-Mania standard-mode",
		"Crm!: Crunch-Mania standard-mode, sampled",
		"CrM2: Crunch-Mania LZH-mode",
		"Crm2: Crunch-Mania LZH-mode, sampled"};
	return names[(_isLZH?2:0)+(_isSampled?1:0)];
}

const std::string &CRMDecompressor::getSubName() const noexcept
{
	// the XPK-id is not used in decompressing process,
	// but there is a real id inside the stream
	// This means we can have frankenstein configurations,
	// although in practice we don't
	static std::string names[2]={
		"XPK-CRM2: Crunch-Mania LZH-mode",
		"XPK-CRMS: Crunch-Mania LZH-mode, sampled"};
	return names[(_isXPKDelta?1:0)];
}

size_t CRMDecompressor::getPackedSize() const noexcept
{
	return _packedSize+14;
}

size_t CRMDecompressor::getRawSize() const noexcept
{
	return _rawSize;
}

void CRMDecompressor::decompressImpl(Buffer &rawData,bool verify)
{
	if (rawData.size()<_rawSize) throw Decompressor::DecompressionError();

	const uint8_t *bufPtr=_packedData.data();
	size_t bufOffset=_packedSize+14-6;

	// There are empty bits?!? at the start of the stream. take them out
	uint32_t originalBitsContent=_packedData.readBE32(bufOffset);
	uint16_t originalShift=_packedData.readBE16(bufOffset+4);
	uint8_t bufBitsLength=originalShift+16;
	uint32_t bufBitsContent=originalBitsContent>>(16-originalShift);

	// streamreader
	auto readBit=[&]()->uint8_t
	{
		if (!bufBitsLength)
		{
			if (bufOffset<=14) throw Decompressor::DecompressionError();
			bufBitsContent=uint32_t(bufPtr[--bufOffset]);
			bufBitsLength=8;
		}
		uint8_t ret=bufBitsContent&1;
		bufBitsContent>>=1;
		bufBitsLength--;
		return ret;
	};

	auto readBits=[&](uint32_t count)->uint32_t
	{
		while (bufBitsLength<count)
		{
			if (bufOffset<=14) throw Decompressor::DecompressionError();
			bufBitsContent|=uint32_t(bufPtr[--bufOffset])<<bufBitsLength;
			bufBitsLength+=8;
		}
		uint32_t ret=bufBitsContent&((1<<count)-1);
		bufBitsContent>>=count;
		bufBitsLength-=count;
		return ret;
	};

	uint8_t *dest=rawData.data();
	size_t destOffset=_rawSize;

	if (_isLZH)
	{
		typedef HuffmanDecoder<uint32_t,0x200,0> CRMHuffmanDecoder;

		auto readHuffmanTable=[&](CRMHuffmanDecoder &dec,uint32_t codeLength)
		{
			uint32_t maxDepth=readBits(4);
			if (!maxDepth) throw Decompressor::DecompressionError();
			uint32_t lengthTable[maxDepth];
			for (uint32_t i=0;i<maxDepth;i++)
				lengthTable[i]=readBits(std::min(i+1,codeLength));
			uint32_t code=0;
			for (uint32_t depth=1;depth<=maxDepth;depth++)
			{
				for (uint32_t i=0;i<lengthTable[depth-1];i++)
				{
						uint32_t value=readBits(codeLength);
						dec.insert(HuffmanCode<uint32_t>{depth,code>>(maxDepth-depth),value});
						code+=1<<(maxDepth-depth);
				}
			}
		};


		do {
			CRMHuffmanDecoder lengthDecoder,distanceDecoder;
			readHuffmanTable(lengthDecoder,9);
			readHuffmanTable(distanceDecoder,4);

			uint32_t items=readBits(16)+1;
			for (uint32_t i=0;i<items;i++)
			{
				uint32_t count=lengthDecoder.decode(readBit);
				if (count&0x100)
				{
					// this is literal, not count
					if (!destOffset) throw Decompressor::DecompressionError();
					dest[--destOffset]=count;
				} else {
					count+=3;

					uint32_t distanceBits=distanceDecoder.decode(readBit);
					uint32_t distance;
					if (!distanceBits)
					{
						distance=readBits(1)+1;
					} else {
						distance=(readBits(distanceBits)|(1<<distanceBits))+1;
					}
					if (destOffset<size_t(count) || destOffset+distance>_rawSize) throw Decompressor::DecompressionError();
					distance+=destOffset;
					for (uint32_t i=0;i<count;i++) dest[--destOffset]=dest[--distance];
				}
			}
		} while (readBit());
	} else {
		HuffmanDecoder<uint8_t,0xffU,3> lengthDecoder
		{
			HuffmanCode<uint8_t>{1,0b000,0},
			HuffmanCode<uint8_t>{2,0b010,1},
			HuffmanCode<uint8_t>{3,0b110,2},
			HuffmanCode<uint8_t>{3,0b111,3}
		};

		HuffmanDecoder<uint8_t,0xffU,2> distanceDecoder
		{
			HuffmanCode<uint8_t>{1,0b00,0},
			HuffmanCode<uint8_t>{2,0b10,1},
			HuffmanCode<uint8_t>{2,0b11,2}
		};

		while (destOffset)
		{
			if (readBit())
			{
				dest[--destOffset]=readBits(8);
			} else {
				uint8_t lengthIndex=lengthDecoder.decode(readBit);

				static const uint8_t lengthBits[4]={1,2,4,8};
				static const uint32_t lengthAdditions[4]={2,4,8,24};
				uint32_t count=readBits(lengthBits[lengthIndex])+lengthAdditions[lengthIndex];
				if (count==23)
				{
					if (readBit())
					{
						count=readBits(5)+15;
					} else {
						count=readBits(14)+15;
					}
					if (count>destOffset) throw Decompressor::DecompressionError();
					for (uint32_t i=0;i<count;i++)
						dest[--destOffset]=readBits(8);
				} else {
					if (count>23) count--;

					uint8_t distanceIndex=distanceDecoder.decode(readBit);
				
					static const uint8_t distanceBits[3]={9,5,14};
					static const uint32_t distanceAdditions[3]={32,0,544};
					uint32_t distance=readBits(distanceBits[distanceIndex])+distanceAdditions[distanceIndex];

					if (!distance || destOffset<count || destOffset+distance>_rawSize) throw Decompressor::DecompressionError();
					distance+=destOffset;
					for (uint32_t i=0;i<count;i++) dest[--destOffset]=dest[--distance];
				}
			}
		}
	}

	if (destOffset) throw Decompressor::DecompressionError();
	if (_isSampled)
		DLTADecode::decode(rawData,rawData,0,_rawSize);
}

void CRMDecompressor::decompressImpl(Buffer &rawData,const Buffer &previousData,bool verify)
{
	if (rawData.size()!=_rawSize) throw Decompressor::DecompressionError();
	return decompressImpl(rawData,verify);
}

Decompressor::Registry<CRMDecompressor> CRMDecompressor::_registration;
XPKDecompressor::Registry<CRMDecompressor> CRMDecompressor::_XPKregistration;
