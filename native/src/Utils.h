#ifndef UTILS_H
#define UTILS_H

namespace godot {
template <typename T>
T min(const T& a1, const T& a2) {
  if (a1 < a2) {
    return a1;
  }
  return a2;
}

template <typename T>
T max(const T& a1, const T& a2) {
  if (a1 > a2) {
    return a1;
  }
  return a2;
}
}  // namespace godot

#endif  // UTILS_H
