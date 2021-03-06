/* Copyright (C) Teemu Suutari */

#ifndef LHLBDECOMPRESSOR_HPP
#define LHLBDECOMPRESSOR_HPP

#include "XPKDecompressor.hpp"

class LHLBDecompressor : public XPKDecompressor
{
public:
	LHLBDecompressor(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify);

	virtual ~LHLBDecompressor();

	virtual const std::string &getSubName() const noexcept override final;

	virtual void decompressImpl(Buffer &rawData,const Buffer &previousData,bool verify) override final;

	static bool detectHeaderXPK(uint32_t hdr) noexcept;
	static std::unique_ptr<XPKDecompressor> create(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify);

private:
	const Buffer	&_packedData;

	static XPKDecompressor::Registry<LHLBDecompressor> _XPKregistration;
};

#endif
