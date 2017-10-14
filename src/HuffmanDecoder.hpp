/* Copyright (C) Teemu Suutari */

#ifndef HUFFMANDECODER_HPP
#define HUFFMANDECODER_HPP

#include <stddef.h>
#include <stdint.h>

#include <vector>

// For exception
#include "Decompressor.hpp"

template<typename T>
struct HuffmanCode
{
	uint32_t	length;
	size_t		code;

	T		value;
};

template<typename T,T emptyValue,size_t depth>
class HuffmanDecoder
{
private:
	static const size_t _length=(2<<depth)-2;

public:
	typedef T ItemType;
	typedef HuffmanCode<T> CodeType;

	HuffmanDecoder(const HuffmanDecoder&)=delete;
	HuffmanDecoder& operator=(const HuffmanDecoder&)=delete;

	HuffmanDecoder()
	{
		for (size_t i=0;i<_length;i++) _table[i]=emptyValue;
	}

	template<typename ...Args>
	HuffmanDecoder(const Args&& ...args) :
		HuffmanDecoder()
	{
		const HuffmanCode<T> list[sizeof...(args)]={args...};
		for (auto &item : list)
			insert(item);
	}

	~HuffmanDecoder()
	{
	}

	void reset()
	{
		for (size_t i=0;i<_length;i++) _table[i]=emptyValue;
	}

	template<typename F>
	T decode(F bitReader) const
	{
		size_t i=0;
		T ret;
		do {
			if (bitReader()) i++;
			ret=_table[i];
			i=i*2+2;
		} while (ret==emptyValue && i<_length);
		if (ret==emptyValue) throw Decompressor::DecompressionError();
		return ret;
	}

	void insert(const HuffmanCode<T> &code)
	{
		if (code.value==emptyValue || code.length>depth) throw Decompressor::DecompressionError();
		for (size_t i=0,j=code.length;j!=0;j--)
		{
			if (code.code&(1<<(j-1))) i++;
			if (_table[i]!=emptyValue) throw Decompressor::DecompressionError();
			if (j==1) _table[i]=code.value;
			i=i*2+2;
		}
	}

private:
	T	_table[_length];
};

// better for non-fixed sizes (especially deep ones)

template<typename T,T emptyValue>
class HuffmanDecoder<T,emptyValue,0>
{
private:
	struct Node
	{
		size_t	sub[2];
		T	value;
	};

public:
	typedef T ItemType;
	typedef HuffmanCode<T> CodeType;

	HuffmanDecoder()
	{
		_table.push_back(Node{{0,0},emptyValue});
	}

	template<typename ...Args>
	HuffmanDecoder(const Args&& ...args) :
		HuffmanDecoder()
	{
		const HuffmanCode<T> list[sizeof...(args)]={args...};
		for (auto &item : list)
			insert(item);
	}

	~HuffmanDecoder()
	{
	}

	void reset()
	{
		_table.clear();
		_table.push_back(Node{{0,0},emptyValue});
	}

	template<typename F>
	T decode(F bitReader) const
	{
		size_t length=_table.size();
		T ret=emptyValue;
		size_t i=0;
		while (i<length && ret==emptyValue)
		{
			i=_table[i].sub[bitReader()?1:0];
			ret=_table[i].value;
			if (!i) break;
		}
		if (ret==emptyValue) throw Decompressor::DecompressionError();
		return ret;
	}

	void insert(const HuffmanCode<T> &code)
	{
		if (code.value==emptyValue) throw Decompressor::DecompressionError();
		size_t i=0,length=_table.size();
		for (int32_t currentBit=code.length;currentBit>=0;currentBit--)
		{
			size_t codeBit=(currentBit)?(code.code&(1<<(currentBit-1)))?1:0:0;
			if (i==length)
			{
				_table.push_back(Node{{(currentBit&&!codeBit)?length+1:0,(currentBit&&codeBit)?length+1:0},currentBit?emptyValue:code.value});
				length++;
				i++;
			} else {
				if (!currentBit || _table[i].value!=emptyValue) throw Decompressor::DecompressionError();
				size_t tmp=_table[i].sub[codeBit];
				if (!tmp)
				{
					_table[i].sub[codeBit]=length;
					i=length;
				} else {
					i=tmp;
				}
			}
		}
	}

private:
	std::vector<Node>	_table;
};

// create orderly Huffman table, as used by Deflate and Bzip2
template<typename T>
void CreateOrderlyHuffmanTable(T &dec,const uint8_t *bitLengths,uint32_t bitTableLength)
{
	uint8_t minDepth=32,maxDepth=0;
	for (uint32_t i=0;i<bitTableLength;i++)
	{
		if (bitLengths[i] && bitLengths[i]<minDepth) minDepth=bitLengths[i];
		if (bitLengths[i]>maxDepth) maxDepth=bitLengths[i];
	}
	if (!maxDepth) throw Decompressor::DecompressionError();

	uint32_t code=0;
	for (uint32_t depth=minDepth;depth<=maxDepth;depth++)
	{
		for (uint32_t i=0;i<bitTableLength;i++)
		{
			if (bitLengths[i]==depth)
			{
				dec.insert(typename T::CodeType{depth,code>>(maxDepth-depth),(typename T::ItemType)i});
				code+=1<<(maxDepth-depth);
			}
		}
	}
}

#endif
