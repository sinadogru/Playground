#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>


using Path = std::string;

template< class... >
using void_t = void;

struct nonesuch {
    nonesuch() = delete;
    ~nonesuch() = delete;
    nonesuch(nonesuch const&) = delete;
    void operator=(nonesuch const&) = delete;
};

namespace detail {
template <class Default, class AlwaysVoid,
          template<class...> class Op, class... Args>
struct detector {
  using value_t = std::false_type;
  using type = Default;
};

template <class Default, template<class...> class Op, class... Args>
struct detector<Default, void_t<Op<Args...>>, Op, Args...> {
  using value_t = std::true_type;
  using type = Op<Args...>;
};

} // namespace detail

template <template<class...> class Op, class... Args>
using is_detected = typename detail::detector<nonesuch, void, Op, Args...>::value_t;

template <template<class...> class Op, class... Args>
using detected_t = typename detail::detector<nonesuch, void, Op, Args...>::type;

template <class Default, template<class...> class Op, class... Args>
using detected_or = detail::detector<Default, void, Op, Args...>;

template <class Expected, template<class...> class Op, class... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template<class T>
using has_size_on_wire =
    decltype(std::declval<T&>().sizeOnWire());


namespace inheritance {
class ResourceMetadata {
public:
  virtual ~ResourceMetadata() = default;
  virtual std::string key() const = 0;
  virtual std::uint32_t sizeOnWire() const = 0;
  virtual void writeToWire(std::ostream& os) const = 0;
};

class RawMetadata : public ResourceMetadata {
public:
  RawMetadata(const std::string& key, const std::string& data)
    : key_(key), data_(data)
  { }

  std::string key() const override
  { return key_; }
  std::uint32_t sizeOnWire() const override
  { return key_.size() + data_.size(); }
  void writeToWire(std::ostream& os) const override
  { os << key_ << data_; }

  std::string data() const
  { return data_; }

private:
  std::string key_;
  std::string data_;
};

class Resource {
public:
  Resource(std::unique_ptr<ResourceMetadata> metadata)
    : metadata_(std::move(metadata))
  { }

  auto metadata() const -> const ResourceMetadata&
  { return *metadata_; }

  auto setMetadata(std::unique_ptr<ResourceMetadata> metadata) -> void
  { metadata_ = std::move(metadata); }

private:
  std::unique_ptr<ResourceMetadata> metadata_;
};
} // namespace inheritance

template<typename T>
std::uint32_t sizeOnStream(const T& obj)
{ return obj.sizeOnStream(); }

template<typename T>
void writeToStream(const T& obj, std::ostream& os)
{ obj.writeToStream(os); }

template<typename MetadataType>
struct MetadataKey;

struct MetadataType {
  friend std::string keyOnStream(const MetadataType&);
  friend std::uint32_t sizeOnStream(const MetadataType&);
  friend void writeToWire(const MetadataType&, std::ostream& os);
};

class ResourceMetadata {
public:
  template<typename MetadataType>
  ResourceMetadata(MetadataType&& metadata)
    : self_(std::make_shared<model<std::remove_reference_t<MetadataType>>>(std::forward<MetadataType>(metadata)))
  { }

  auto print(std::ostream& os) const -> void
  { self_->print(os); }

  auto key() const -> std::string
  { return self_->keyOnStream(); }

  auto sizeOnStream() const -> std::uint32_t
  { return self_->sizeOnStream(); }

  auto writeToStream(std::ostream& os) const -> void
  { self_->writeToStream(os); }

  auto typeInfo() const -> const std::type_info&
  { return self_->type_info(); }

  template<typename Type>
  friend auto get(const ResourceMetadata& meta) -> const Type*
  {
    auto object = dynamic_cast<const model<Type>*>(meta.self_.get());
    if (object)
      return &object->data_;
    return nullptr;
  }

private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual void print(std::ostream& os) const = 0;
    virtual std::string keyOnStream() const = 0;
    virtual std::uint32_t sizeOnStream() const = 0;
    virtual void writeToStream(std::ostream& os) const = 0;
    virtual const std::type_info& type_info() const = 0;
  };

  template<typename T>
  struct model : concept_t
  {
    template<typename U>
    model(U&& x)
      : data_(std::forward<U>(x))
    { }

    const std::type_info& type_info() const override
    { return typeid(data_); }

    std::string keyOnStream() const override
    { return T::Key; }

    void print(std::ostream& os) const
    { data_.print(os); }

    std::uint32_t sizeOnStream() const override
    { return ::sizeOnStream(data_); }

    void writeToStream(std::ostream& os) const override
    { ::writeToStream(data_, os); }

    T data_;
  };

  std::shared_ptr<const concept_t> self_;
};

class Resource {
public:
  Resource(ResourceMetadata metadata)
    : metadata_(std::move(metadata))
  { }

  auto metadata() const -> ResourceMetadata
  { return metadata_; }

  auto setMetadata(ResourceMetadata metadata) -> void
  { metadata_ = std::move(metadata); }

private:
  ResourceMetadata metadata_;
};

class RawMetadata { // What about calling this as UnknownMetadata
public:
  std::uint32_t sizeOnStream() const
  { return key_.size() + data_.size(); }

  void writeToStream(std::ostream& os) const
  { os << data_; }

  void print(std::ostream& os) const
  { os << "Raw data: " << data_; }

  static const std::string Key;

  std::string key() const
  { return key_; }

  std::string data() const
  { return data_; }

private:
  friend class MetadataFactory;

  RawMetadata(const std::string& key, const std::string& data)
    : key_(key), data_(data)
  { }

  std::string key_;
  std::string data_;
};

const std::string RawMetadata::Key = "RawMetadata";

class MetadataFactory {
public:
  using Creator = std::function<ResourceMetadata(std::istream&)>;

  bool registerKey(const std::string& key, Creator creator)
  { return associations_.insert(AssocMap::value_type(key, creator)).second; }

  bool unregisterKey(const std::string& key)
  { return associations_.erase(key) == 1; }

  template<typename MetadataType>
  bool registerType(Creator creator)
  { return registerKey(MetadataType::Key, std::move(creator)); }

  template<typename MetadataType>
  bool unregisterType(Creator creator)
  { return unregisterKey(MetadataType::Key, std::move(creator)); }

  ResourceMetadata createMetadata(const std::string& key, std::istream& is)
  {
    auto it = associations_.find(key);
    if (it != associations_.end())
      return it->second(is);
    return createRawMetadata(key, is);
  }

  template<typename MetadataType>
  ResourceMetadata createMetadata(std::istream& is)
  { return createMetadata(MetadataType::Key, is); }

private:
  RawMetadata createRawMetadata(const std::string& key, std::istream& is)
  {
    return RawMetadata(key, {std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>()});
  }

  using AssocMap = std::map<std::string, Creator>;
  AssocMap associations_;
};

class SongMetadata {
public:
  SongMetadata(const std::string& songName)
    : songName_(songName)
  { }

  static const std::string Key;

  std::string songName() const
  { return songName_; }

  void print(std::ostream& os) const
  { os << "Song Name: " << songName_; }

private:
  std::string songName_;
};

const std::string SongMetadata::Key = "SongMetadata";

std::uint32_t sizeOnStream(const SongMetadata& obj)
{ return obj.songName().size(); }

void writeToStream(const SongMetadata& obj, std::ostream& os)
{ os << obj.songName(); }


int main()
{
  auto songCreator = [](std::istream& is) {
    std::string songName;
    is >> songName;
    return SongMetadata(songName);
  };

  MetadataFactory factory;
  factory.registerType<SongMetadata>(songCreator);

  std::stringstream ss;
  {
    SongMetadata song("HeyDJ");
    ResourceMetadata meta(std::move(song));
    meta.writeToStream(ss);
  }

  auto meta = factory.createMetadata<SongMetadata>(ss);

  std::cout << "key of meta: " << meta.key() << ". Size on stream: " << meta.sizeOnStream() << std::endl;
  auto s = get<SongMetadata>(meta);
  std::cout << "Name of song: " << s->songName() << std::endl;

  std::cout << "creating resource, " << std::endl;
  Resource resource(std::move(meta));
  resource.metadata().print(std::cout);
  std::cout << std::endl;
}
