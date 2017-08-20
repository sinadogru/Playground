#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>


template <typename I, // I models RandomAccessIterator
          typename C, // C models StrictWeakOrdering(T, T)
          typename P> // P models UnaryFunction(value_type(I)) -> T
inline void sort(I f, I l, C c, P p) {
  return std::sort(f, l, std::bind(c, std::bind(p, std::placeholders::_1),
                                   std::bind(p, std::placeholders::_2)));
}

struct MyStruct {
public:
  MyStruct() = default;
  MyStruct(int id) : mId{id} {}

  int id() const { return mId; }

private:
  int mId;
};


int main()
{
  const auto printIds = [](const auto& ids) {
    for (const auto& e : ids)
      std::cout << e.id() << ' ';
    std::cout << std::endl;
  };

  std::vector<int> v(16);
  std::iota(v.begin(), v.end(), 1);
  std::shuffle(v.begin(), v.end(), std::mt19937{std::random_device{}()});

  std::vector<MyStruct> ids(16);
  std::transform(v.begin(), v.end(), ids.begin(), [](int id){ return MyStruct{id}; });

  printIds(ids);

  sort(ids.begin(), ids.end(), std::less<>(), &MyStruct::id);

  printIds(ids);
}
