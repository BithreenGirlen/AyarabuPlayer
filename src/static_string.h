#ifndef STATIC_STRING_H_
#define STATIC_STRING_H_

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (defined(__cplusplus) && __cplusplus >= 201703L)
#define ALLOW_USING_STRING_VIEW
#include <string_view>
#else
#include <stdint.h>
#endif

/// @brief 動的割り当てを行わない文字列操作
template<typename T, size_t N>
class StaticString
{
public:
	constexpr StaticString() noexcept = default;
	template <size_t M>
	constexpr StaticString(const T(&s)[M]) noexcept
	{
		constexpr size_t length = M - 1;
		static_assert(length <= MaxSize, "String literal is longer than the capacity of StaticString.");

		for (size_t i = 0; i < length; ++i)
		{
			m_data[i] = s[i];
		}

		m_nWritten = length;
		m_data[m_nWritten] = s_termination;
	}

	constexpr const T* data() const noexcept { return m_data; }
	constexpr size_t size() const noexcept { return m_nWritten; }
	constexpr size_t available() const noexcept { return MaxSize - m_nWritten; }
	constexpr size_t capacity() const noexcept { return MaxSize; }
	constexpr bool empty() const noexcept { return m_nWritten == 0; }
	constexpr const T& front() const noexcept { return m_data[0]; }
	constexpr const T& back() const noexcept { return m_nWritten == 0 ? m_data[0] : m_data[m_nWritten - 1]; }
	constexpr T& front() noexcept { return m_data[0]; }
	constexpr T& back() noexcept { return m_nWritten == 0 ? m_data[0] : m_data[m_nWritten - 1]; }

#if defined(ALLOW_USING_STRING_VIEW)
	constexpr std::basic_string_view<T> string_view() const noexcept
	{
		return std::basic_string_view<T>(m_data, m_nWritten);
	}
#endif
#if defined(ALLOW_USING_STRING_VIEW)
	/// @brief 文字列割り当て
	StaticString& assign(std::basic_string_view<T> s) noexcept
	{
		if (s.size() > MaxSize)return *this;

		::memcpy(m_data, s.data(), s.size() * sizeof(T));
		m_nWritten = s.size();
		m_data[m_nWritten] = s_termination;

		return *this;
	}
#else
	template <size_t M>
	StaticString& assign(const T(&s)[M]) noexcept
	{
		constexpr size_t length = M - 1;
		if (length > MaxSize)return *this;

		::memcpy(m_data, s, length * sizeof(T));
		m_nWritten = length;
		m_data[m_nWritten] = s_termination;

		return *this;
	}
#endif
	StaticString& assign(const StaticString& s) noexcept
	{
		if (s.size() > MaxSize)return *this;

		::memcpy(m_data, s.data(), s.size() * sizeof(T));
		m_nWritten = s.size();
		m_data[m_nWritten] = s_termination;

		return *this;
	}
	StaticString& assign(const T* s, size_t length) noexcept
	{
		if (length > MaxSize)return *this;

		::memcpy(m_data, s, length * sizeof(T));
		m_nWritten = length;
		m_data[m_nWritten] = s_termination;

		return *this;
	}
#if defined(ALLOW_USING_STRING_VIEW)
	/// @brief 文字列連結
	StaticString& append(std::basic_string_view<T> s) noexcept
	{
		if (m_nWritten + s.size() > MaxSize)return *this;

		::memcpy(m_data + m_nWritten, s.data(), s.size() * sizeof(T));
		m_nWritten += s.size();
		m_data[m_nWritten] = s_termination;

		return *this;
	}
#else
	template <size_t M>
	StaticString& append(const T(&s)[M]) noexcept
	{
		constexpr size_t length = M - 1;
		if (m_nWritten + length > MaxSize)return *this;

		::memcpy(m_data + m_nWritten, s, length * sizeof(T));
		m_nWritten += length;
		m_data[m_nWritten] = s_termination;

		return *this;
	}
#endif
	StaticString& append(const StaticString& s) noexcept
	{
		if (m_nWritten + s.size() > MaxSize)return *this;

		::memcpy(m_data + m_nWritten, s.data(), s.size() * sizeof(T));
		m_nWritten = s.size();
		m_data[m_nWritten] = s_termination;

		return *this;
	}
	StaticString& append(const T* s, size_t length) noexcept
	{
		if (m_nWritten + length > MaxSize)return *this;

		::memcpy(m_data + m_nWritten, s, length * sizeof(T));
		m_nWritten += length;
		m_data[m_nWritten] = s_termination;

		return *this;
	}

	/// @brief 文字連結
	void push_back(const T c) noexcept
	{
		if (m_nWritten + 1 > MaxSize)return;

		m_data[m_nWritten] = c;
		++m_nWritten;
		m_data[m_nWritten] = s_termination;
	}

	/// @brief 文字挿入
	void insert(const T c, size_t nPos = 0) noexcept
	{
		if (m_nWritten + 1 > MaxSize)return;

		::memmove(&m_data[nPos + 1], &m_data[nPos], (m_nWritten - nPos) * sizeof(T));
		m_data[nPos] = c;
		++m_nWritten;
	}
#if defined(ALLOW_USING_STRING_VIEW)
	/// @brief 文字列挿入
	void insert(std::basic_string_view<T> s, size_t nPos = 0) noexcept
	{
		if (s.size() + nPos > MaxSize)return;

		::memmove(&m_data[nPos + s.size()], &m_data[nPos], (m_nWritten - nPos) * sizeof(T));
		::memcpy(&m_data[nPos], s.data(), s.size() * sizeof(T));
		m_nWritten += s.size();
	}
#else
	template <size_t M>
	void insert(const T(&s)[M], size_t nPos = 0) noexcept
	{
		const size_t length = M - 1;
		if (length > MaxSize)return;

		::memmove(&m_data[nPos + length], &m_data[nPos], (m_nWritten - nPos) * sizeof(T));
		::memcpy(&m_data[nPos], s, length * sizeof(T));
		m_nWritten += length;
	}
#endif
	/// @brief 文字置換
	void replace(const T cOld, const T cNew) noexcept
	{
		for (size_t i = 0; i < m_nWritten; ++i)
		{
			T& cRef = m_data[i];
			if (cRef == cOld)
			{
				cRef = cNew;
			}
		}
	}
#if defined(ALLOW_USING_STRING_VIEW)
	/// @brief 文字列置換
	void replace(std::basic_string_view<T> strOld, std::basic_string_view<T> strNew) noexcept
	{
		if (strOld.empty())return;

		for (size_t nLast = 0; nLast < m_nWritten;)
		{
			std::basic_string_view<T> s = string_view();
			size_t nPos = s.find(strOld, nLast);
			if (nPos == std::basic_string_view<T>::npos)break;

			ptrdiff_t nDiff = static_cast<ptrdiff_t>(strNew.size()) - static_cast<ptrdiff_t>(strOld.size());
			if (nDiff > 0 && m_nWritten + static_cast<size_t>(nDiff) > MaxSize) break;

			T* pPos = m_data + nPos;
			::memmove(pPos + strNew.size(), pPos + strOld.size(), (m_nWritten - nPos - strOld.size() + 1) * sizeof(T));
			::memcpy(pPos, strNew.data(), strNew.size() * sizeof(T));

			m_nWritten += nDiff;
			nLast = nPos + strNew.size();
		}
	}
#else
	template<size_t sizeOld, size_t sizeNew>
	void replace(const T(&sOld)[sizeOld], const T(&sNew)[sizeNew]) noexcept
	{
		constexpr size_t lenOld = sizeOld - 1;
		constexpr size_t lenNew = sizeNew - 1;

		if (lenOld == 0)return;

		for (size_t nLast = 0; nLast < m_nWritten;)
		{
			size_t nPos = nLast;

			for (; nPos + lenOld <= m_nWritten; ++nPos)
			{
				size_t i = 0;
				for (; i < lenOld; ++i)
				{
					if (m_data[nPos + i] != sOld[i])break;
				}

				if (i == lenOld)break;
			}

			if (nPos + lenOld > m_nWritten)break;

			ptrdiff_t nDiff = static_cast<ptrdiff_t>(lenNew) - static_cast<ptrdiff_t>(lenOld);
			if (nDiff > 0 && m_nWritten + static_cast<size_t>(nDiff) > MaxSize) break;

			T* pPos = m_data + nPos;
			::memmove(pPos + lenNew, pPos + lenOld, (m_nWritten - nPos - lenOld + 1) * sizeof(T));
			::memcpy(pPos, sNew, lenNew * sizeof(T));

			m_nWritten += nDiff;
			nLast = nPos + lenNew;
		}
	}
#endif
	/// @brief 消去
	void clear() noexcept
	{
		::memset(m_data, s_termination, MaxSize * sizeof(T));
		m_nWritten = 0;
	}
	/// @brief 縮め
	void shrink(size_t nLength) noexcept
	{
		if (nLength >= m_nWritten)return;

		::memset(m_data + nLength, s_termination, (MaxSize - nLength) * sizeof(T));
		m_nWritten = nLength;
	}
	void resize(size_t nLength) noexcept
	{
		if (nLength >= MaxSize)return;

		m_nWritten = nLength;
		m_data[m_nWritten] = s_termination;
	}

	T& operator[](const size_t nPos) noexcept
	{
		return m_data[nPos];
	}
#if defined(ALLOW_USING_STRING_VIEW)
	constexpr bool operator==(const StaticString& other) const noexcept
	{
		return string_view() == other.string_view();
	}
#else
	bool operator==(const StaticString& other) const noexcept
	{
		if (m_nWritten != other.m_nWritten)return false;

		return ::memcmp(m_data, other.m_data, m_nWritten);
	}
#endif
private:
	T m_data[N]{};
	size_t m_nWritten = 0;
	static constexpr size_t MaxSize = sizeof(m_data) / sizeof(T) - 1;

#if defined(ALLOW_USING_STRING_VIEW)
	static constexpr T s_termination = []()
		-> T
		{
			if constexpr (std::is_same_v<T, char>) return '\0';
			else if constexpr (std::is_same_v<T, wchar_t>) return L'\0';
			else if constexpr (std::is_same_v<T, char16_t>) return u'\0';
			else if constexpr (std::is_same_v<T, char32_t>) return U'\0';
		}();
#else
	static constexpr T s_termination = T();
#endif
};

using StaticString512 = StaticString<char, 512>;
using StaticWString512 = StaticString<wchar_t, 512>;

#endif // !STATIC_STRING_H_
