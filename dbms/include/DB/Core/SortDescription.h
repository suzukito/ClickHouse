#pragma once

#include <vector>

#include <DB/Core/Types.h>
#include <DB/Core/Block.h>
#include <DB/Columns/IColumn.h>
#include <DB/Columns/ColumnString.h>
#include <DB/Common/Collator.h>


namespace DB
{

/// Описание правила сортировки по одному столбцу.
struct SortColumnDescription
{
	String column_name;						/// Имя столбца.
	size_t column_number;					/// Номер столбца (используется, если не задано имя).
	int direction;							/// 1 - по возрастанию, -1 - по убыванию.
	Poco::SharedPtr<Collator> collator;	/// Collator для locale-specific сортировки строк

	SortColumnDescription(size_t column_number_, int direction_, const Poco::SharedPtr<Collator> & collator_ = NULL)
		: column_number(column_number_), direction(direction_), collator(collator_) {}

	SortColumnDescription(String column_name_, int direction_, const Poco::SharedPtr<Collator> & collator_ = NULL)
		: column_name(column_name_), column_number(0), direction(direction_), collator(collator_) {}

	/// Для IBlockInputStream.
	String getID() const
	{
		std::stringstream res;
		res << column_name << ", " << column_number << ", " << direction;
		if (!collator.isNull())
			res << ", collation locale: " << collator->getLocale();
		return res.str();
	}
};

/// Описание правила сортировки по нескольким столбцам.
typedef std::vector<SortColumnDescription> SortDescription;


/** Курсор, позволяющий сравнивать соответствующие строки в разных блоках.
  * Курсор двигается по одному блоку.
  * Для использования в priority queue.
  */
struct SortCursorImpl
{
	ConstColumnPlainPtrs all_columns;
	ConstColumnPlainPtrs sort_columns;
	SortDescription desc;
	size_t sort_columns_size;
	size_t pos;
	size_t rows;

	/** Порядок (что сравнивается), если сравниваемые столбцы равны.
	  * Даёт возможность предпочитать строки из нужного курсора.
	  */
	size_t order;
	
	typedef std::vector<UInt8> NeedCollationFlags;
	
	/** Нужно ли использовать Collator для сортировки столбца */
	NeedCollationFlags need_collation;
	
	/** Есть ли хотя бы один столбец с Collator. */
	bool has_collation;
	
	SortCursorImpl() {}

	SortCursorImpl(const Block & block, const SortDescription & desc_, size_t order_ = 0)
		: desc(desc_), sort_columns_size(desc.size()), order(order_), need_collation(desc.size()), has_collation(false)
	{
		reset(block);
	}

	/// Установить курсор в начало нового блока.
	void reset(const Block & block)
	{
		all_columns.clear();
		sort_columns.clear();
		
		size_t num_columns = block.columns();

		for (size_t j = 0; j < num_columns; ++j)
			all_columns.push_back(&*block.getByPosition(j).column);

		for (size_t j = 0, size = desc.size(); j < size; ++j)
		{
			size_t column_number = !desc[j].column_name.empty()
				? block.getPositionByName(desc[j].column_name)
				: desc[j].column_number;

			sort_columns.push_back(&*block.getByPosition(column_number).column);
			
			need_collation[j] = !desc[j].collator.isNull() && sort_columns.back()->getName() == "ColumnString";
			has_collation |= need_collation[j];
		}

		pos = 0;
		rows = all_columns[0]->size();
	}

	bool isLast() const { return pos + 1 >= rows; }
	void next() { ++pos; }
};


/// Для лёгкости копирования.
struct SortCursor
{
	SortCursorImpl * impl;

	SortCursor(SortCursorImpl * impl_) : impl(impl_) {}
	SortCursorImpl * operator-> () { return impl; }
	const SortCursorImpl * operator-> () const { return impl; }

	/// Инвертировано, чтобы из priority queue элементы вынимались в нужном порядке.
	bool operator< (const SortCursor & rhs) const
	{
		for (size_t i = 0; i < impl->sort_columns_size; ++i)
		{
			int res = impl->desc[i].direction * impl->sort_columns[i]->compareAt(impl->pos, rhs.impl->pos, *(rhs.impl->sort_columns[i]));
			if (res > 0)
				return true;
			if (res < 0)
				return false;
		}
		return impl->order > rhs.impl->order;
	}
};


/// Отдельный компаратор для locale-sensitive сравнения строк
struct SortCursorWithCollation
{
	SortCursorImpl * impl;

	SortCursorWithCollation(SortCursorImpl * impl_) : impl(impl_) {}
	SortCursorImpl * operator-> () { return impl; }
	const SortCursorImpl * operator-> () const { return impl; }

	/// Инвертировано, чтобы из priority queue элементы вынимались в нужном порядке.
	bool operator< (const SortCursorWithCollation & rhs) const
	{
		for (size_t i = 0; i < impl->sort_columns_size; ++i)
		{
			int res;
			if (impl->need_collation[i])
			{
				const ColumnString & column_string = dynamic_cast<const ColumnString &>(*impl->sort_columns[i]);
				res = column_string.compareAt(impl->pos, rhs.impl->pos, *(rhs.impl->sort_columns[i]), *impl->desc[i].collator);
			}
			else
				res = impl->sort_columns[i]->compareAt(impl->pos, rhs.impl->pos, *(rhs.impl->sort_columns[i]));
			
			res *= impl->desc[i].direction;
			if (res > 0)
				return true;
			if (res < 0)
				return false;
		}
		return impl->order > rhs.impl->order;
	}
};

}

