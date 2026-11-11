#pragma once

#include "core/ds/DynArray.h"
#include <cstddef>
#include <utility>

template<class T, class Cmp>
void bubbleSort(T* first, T* last, Cmp cmp) {
    if (first == last) return;
    for (T* end = last; end - first > 1; --end) {
        bool changed = false;
        for (T* p = first; p + 1 < end; ++p)
            if (cmp(*(p + 1), *p)) { std::swap(*p, *(p + 1)); changed = true; }
        if (!changed) break;
    }
}

template<class T, class Cmp>
void insertionSort(T* first, T* last, Cmp cmp) {
    for (T* p = first + (first != last); p < last; ++p) {
        T value = std::move(*p);
        T* j = p;
        while (j > first && cmp(value, *(j - 1))) { *j = std::move(*(j - 1)); --j; }
        *j = std::move(value);
    }
}

template<class T, class Cmp>
void selectionSort(T* first, T* last, Cmp cmp) {
    for (T* p = first; p < last; ++p) {
        T* best = p;
        for (T* q = p + 1; q < last; ++q) if (cmp(*q, *best)) best = q;
        if (best != p) std::swap(*p, *best);
    }
}

template<class T, class Cmp>
void shellSort(T* first, T* last, Cmp cmp) {
    std::ptrdiff_t n = last - first;
    for (std::ptrdiff_t gap = n / 2; gap > 0; gap /= 2) {
        for (std::ptrdiff_t i = gap; i < n; ++i) {
            T value = std::move(first[i]);
            std::ptrdiff_t j = i;
            while (j >= gap && cmp(value, first[j - gap])) {
                first[j] = std::move(first[j - gap]); j -= gap;
            }
            first[j] = std::move(value);
        }
    }
}

template<class T, class Cmp>
void mergeSortImpl(T* a, T* temp, std::size_t left, std::size_t right, Cmp cmp) {
    if (right - left < 2) return;
    std::size_t mid = left + (right - left) / 2;
    mergeSortImpl(a, temp, left, mid, cmp);
    mergeSortImpl(a, temp, mid, right, cmp);
    std::size_t i = left, j = mid, k = left;
    while (i < mid && j < right)
        temp[k++] = cmp(a[j], a[i]) ? a[j++] : a[i++];
    while (i < mid) temp[k++] = a[i++];
    while (j < right) temp[k++] = a[j++];
    for (k = left; k < right; ++k) a[k] = std::move(temp[k]);
}

template<class T, class Cmp>
void mergeSort(T* first, T* last, Cmp cmp) {
    std::size_t n = static_cast<std::size_t>(last - first);
    if (n < 2) return;
    T* temp = new T[n];
    mergeSortImpl(first, temp, 0, n, cmp);
    delete[] temp;
}

template<class T, class Cmp>
void quickSortImpl(T* a, std::ptrdiff_t lo, std::ptrdiff_t hi, Cmp cmp) {
    while (lo < hi) {
        std::ptrdiff_t i = lo, j = hi;
        T pivot = a[lo + (hi - lo) / 2];
        while (i <= j) {
            while (cmp(a[i], pivot)) ++i;
            while (cmp(pivot, a[j])) --j;
            if (i <= j) { std::swap(a[i], a[j]); ++i; --j; }
        }
        if (j - lo < hi - i) {
            if (lo < j) quickSortImpl(a, lo, j, cmp);
            lo = i;
        } else {
            if (i < hi) quickSortImpl(a, i, hi, cmp);
            hi = j;
        }
    }
}

template<class T, class Cmp>
void quickSort(T* first, T* last, Cmp cmp) {
    if (last - first > 1) quickSortImpl(first, 0, last - first - 1, cmp);
}

template<class T, class Cmp>
void bubbleSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) bubbleSort(a.begin(), a.end(), cmp); }
template<class T, class Cmp>
void insertionSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) insertionSort(a.begin(), a.end(), cmp); }
template<class T, class Cmp>
void selectionSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) selectionSort(a.begin(), a.end(), cmp); }
template<class T, class Cmp>
void shellSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) shellSort(a.begin(), a.end(), cmp); }
template<class T, class Cmp>
void mergeSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) mergeSort(a.begin(), a.end(), cmp); }
template<class T, class Cmp>
void quickSort(DynArray<T>& a, Cmp cmp) { if (a.size() > 1) quickSort(a.begin(), a.end(), cmp); }
