#pragma once

#include <vector>
#include <array>
#include <deque>


template <typename T>
struct SDeque : std::deque<T> {
	int count = 0;

	typedef std::deque<T> Self;

	void reserve(int count) {
		 Self::resize(count);
	}

	void push_back(const T& item) {
		if (Self::size() == count + 1) {
			Self::resize(count + 1024);
		}
		this->operator[](count++) = item;
	}

	
	template <typename ... Q>
	void emplace_back(Q && ... item) {
		if (Self::size() == count + 1) {
			Self::resize(count + 1024);
		}
		new (&this->operator[](count))T(std::forward<Q>(item)...);
		++count;
		// this->operator[](count++) = item;
	}


	T &back() {
		return this->operator[](count - 1);
	}

	int size() const {
		return count;
	}
};

