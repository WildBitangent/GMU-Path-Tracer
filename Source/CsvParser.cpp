#include "CsvParser.hpp"
#include <sstream>

std::string const& CSVRow::operator[](std::size_t index) const
{
	return mData[index];
}

std::size_t CSVRow::size() const
{
	return mData.size();
}

void CSVRow::readRow(std::istream& str)
{
	mData.clear();
	
	std::string line;
	std::getline(str, line);

	std::stringstream lineStream(line);
	std::string token;

	while(std::getline(lineStream, token, ','))
	    mData.emplace_back(token);
	
	if (!lineStream && token.empty())
	    mData.emplace_back();
}

CSVIterator::CSVIterator(std::istream& str)
  : mString(str.good() ? &str : nullptr)
{
	++(*this);
}

CSVIterator& CSVIterator::operator++()
{
	if (mString)
	{
		if (!(*mString >> mRow))
			mString = nullptr;
	}

	return *this;
}

CSVIterator CSVIterator::operator++(int)
{
	CSVIterator tmp(*this);
	++(*this);
	return tmp;
}

CSVRow const& CSVIterator::operator*() const
{
	return mRow;
}

CSVRow const* CSVIterator::operator->() const
{
	return &mRow;
}

bool CSVIterator::operator==(CSVIterator const& rhs)
{
	return this == &rhs || (this->mString == nullptr && rhs.mString == nullptr);
}

bool CSVIterator::operator!=(CSVIterator const& rhs)
{
	return !(*this == rhs);
}

CSVIterator::operator bool() const
{
	return mString != nullptr;
}


