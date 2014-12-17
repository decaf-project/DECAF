/**
 * @file Template class for storing a "history"
 * @Author Lok Yan
 */
#ifndef HISTORY_H
#define HISTORY_H

#include <deque>
#include <string>
#include "trace_x86.h"

/**
 * Uses a deque to store the most recent maxLines T items in a history.
 */
template <class T>
class History
{
private:
  std::deque<T> history;
  size_t maxLines;
  T empty;

public:
  /**
   * Default constructor that sets history size to 10
   */
  History() : maxLines(10) {}

  /**
   * Constructor for setting what is returned if history is empty
   * @param newEmpty
   */
  History(const T& newEmpty) : maxLines(10), empty(newEmpty) {}
 
  /**
   * Sets what to return if "empty"
   * @param newEmpty
   */
  void setEmpty(const T& newEmpty) { empty = newEmpty; }

  /**
   * @return True if the history is empty
   */
  bool isEmpty() const { return history.empty(); }

  /**
   * Sets the maximum size.
   * @param i The new maximum size
   */
  void setMaxSize(size_t i) { maxLines = i; }
  
  /**
   * @return Current maximum size
   */
  size_t getMaxSize() const { return (maxLines); }

  /**
   * @return Number of items in the history
   */
  size_t size() const { return history.size(); }

  /**
   * Clears the history
   */
  void clear() { history.clear(); }

  /**
   * Push a new item into the back of the history
   * @param s The new item to push in
   */
  void push(const T& s);

  /**
   * Gets the item at index i. Returns "empty" if i is outside of the boundaries
   * @param i The index
   * @return Const reference to the item.
   */
  const T& at(size_t i) const;

  /**
   * Truncates the history to the number of elements specified
   *
   */
  void truncate(size_t s) { history.resize(s); }
};

template <class T>
void History<T>::push(const T& s)
{
  while (history.size() >= maxLines)
  {
    history.pop_back();
  }
  history.push_front(s);
}

template <class T>
const T& History<T>::at(size_t i) const
{
  if (i >= history.size())
  {
    return empty;
  }

  return (history.at(i));
}

#endif//HISTORY_H
