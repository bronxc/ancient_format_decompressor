/* Copyright (C) Teemu Suutari */

#ifndef LZXDECOMPRESSOR_HPP
#define LZXDECOMPRESSOR_HPP

#include "XPKDecompressor.hpp"

class LZXDecompressor : public XPKDecompressor
{
public:
	LZXDecompressor(uint32_t hdr,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state);
	virtual ~LZXDecompressor();

	virtual bool isValid() const override final;
	virtual bool verifyPacked() const override final;
	virtual bool verifyRaw(const Buffer &rawData) const override final;

	virtual const std::string &getSubName() const override final;

	virtual bool decompress(Buffer &rawData,const Buffer &previousData) override final;

	static bool detectHeaderXPK(uint32_t hdr);
	static bool isRecursive();
	static std::unique_ptr<XPKDecompressor> create(uint32_t hdr,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state);

private:
	const Buffer &_packedData;

	bool		_isValid=false;
	bool		_isSampled=false;
	bool		_isCompressed=false;
	size_t		_packedSize=0;
	size_t		_packedOffset=0;
	size_t		_rawSize=0;
	uint32_t	_rawCRC;
};

#endif