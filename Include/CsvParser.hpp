#pragma once
#include <string>
#include <vector>

class CSVRow
{
public:
	std::string const& operator[](std::size_t index) const;
	[[nodiscard]] std::size_t size() const;
	void readRow(std::istream& str);
	
private:
	std::vector<std::string> mData;
};

inline std::istream& operator>>(std::istream& str, CSVRow& data)
{
    data.readRow(str);
    return str;
}  

class CSVIterator
{   
public:
	using iterator_category = std::input_iterator_tag;
	using value_type = CSVRow;
	using difference_type = size_t;
	using pointer = CSVRow*;
	using reference = CSVRow&;
	
	CSVIterator() = default;
	CSVIterator(std::istream& str);

	CSVIterator& operator++();
	CSVIterator operator++(int);
	CSVRow const& operator*() const;
	CSVRow const* operator->() const;

	bool operator==(CSVIterator const& rhs);
	bool operator!=(CSVIterator const& rhs);

	operator bool() const;
private:
	std::istream*       mString = nullptr;
	CSVRow              mRow;
};
