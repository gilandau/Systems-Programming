// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: DATA_PACKET.proto

#ifndef PROTOBUF_INCLUDED_DATA_5fPACKET_2eproto
#define PROTOBUF_INCLUDED_DATA_5fPACKET_2eproto

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3006001
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3006001 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_table_driven.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/inlined_string_field.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)
#define PROTOBUF_INTERNAL_EXPORT_protobuf_DATA_5fPACKET_2eproto 

namespace protobuf_DATA_5fPACKET_2eproto {
// Internal implementation detail -- do not use these members.
struct TableStruct {
  static const ::google::protobuf::internal::ParseTableField entries[];
  static const ::google::protobuf::internal::AuxillaryParseTableField aux[];
  static const ::google::protobuf::internal::ParseTable schema[1];
  static const ::google::protobuf::internal::FieldMetadata field_metadata[];
  static const ::google::protobuf::internal::SerializationTable serialization_table[];
  static const ::google::protobuf::uint32 offsets[];
};
void AddDescriptors();
}  // namespace protobuf_DATA_5fPACKET_2eproto
class Packet;
class PacketDefaultTypeInternal;
extern PacketDefaultTypeInternal _Packet_default_instance_;
namespace google {
namespace protobuf {
template<> ::Packet* Arena::CreateMaybeMessage<::Packet>(Arena*);
}  // namespace protobuf
}  // namespace google

// ===================================================================

class Packet : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:Packet) */ {
 public:
  Packet();
  virtual ~Packet();

  Packet(const Packet& from);

  inline Packet& operator=(const Packet& from) {
    CopyFrom(from);
    return *this;
  }
  #if LANG_CXX11
  Packet(Packet&& from) noexcept
    : Packet() {
    *this = ::std::move(from);
  }

  inline Packet& operator=(Packet&& from) noexcept {
    if (GetArenaNoVirtual() == from.GetArenaNoVirtual()) {
      if (this != &from) InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }
  #endif
  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _internal_metadata_.unknown_fields();
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return _internal_metadata_.mutable_unknown_fields();
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const Packet& default_instance();

  static void InitAsDefaultInstance();  // FOR INTERNAL USE ONLY
  static inline const Packet* internal_default_instance() {
    return reinterpret_cast<const Packet*>(
               &_Packet_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  void Swap(Packet* other);
  friend void swap(Packet& a, Packet& b) {
    a.Swap(&b);
  }

  // implements Message ----------------------------------------------

  inline Packet* New() const final {
    return CreateMaybeMessage<Packet>(NULL);
  }

  Packet* New(::google::protobuf::Arena* arena) const final {
    return CreateMaybeMessage<Packet>(arena);
  }
  void CopyFrom(const ::google::protobuf::Message& from) final;
  void MergeFrom(const ::google::protobuf::Message& from) final;
  void CopyFrom(const Packet& from);
  void MergeFrom(const Packet& from);
  void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input) final;
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const final;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* target) const final;
  int GetCachedSize() const final { return _cached_size_.Get(); }

  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(Packet* other);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return NULL;
  }
  inline void* MaybeArenaPtr() const {
    return NULL;
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // repeated string arg = 7;
  int arg_size() const;
  void clear_arg();
  static const int kArgFieldNumber = 7;
  const ::std::string& arg(int index) const;
  ::std::string* mutable_arg(int index);
  void set_arg(int index, const ::std::string& value);
  #if LANG_CXX11
  void set_arg(int index, ::std::string&& value);
  #endif
  void set_arg(int index, const char* value);
  void set_arg(int index, const char* value, size_t size);
  ::std::string* add_arg();
  void add_arg(const ::std::string& value);
  #if LANG_CXX11
  void add_arg(::std::string&& value);
  #endif
  void add_arg(const char* value);
  void add_arg(const char* value, size_t size);
  const ::google::protobuf::RepeatedPtrField< ::std::string>& arg() const;
  ::google::protobuf::RepeatedPtrField< ::std::string>* mutable_arg();

  // repeated bytes drivedata = 10;
  int drivedata_size() const;
  void clear_drivedata();
  static const int kDrivedataFieldNumber = 10;
  const ::std::string& drivedata(int index) const;
  ::std::string* mutable_drivedata(int index);
  void set_drivedata(int index, const ::std::string& value);
  #if LANG_CXX11
  void set_drivedata(int index, ::std::string&& value);
  #endif
  void set_drivedata(int index, const char* value);
  void set_drivedata(int index, const void* value, size_t size);
  ::std::string* add_drivedata();
  void add_drivedata(const ::std::string& value);
  #if LANG_CXX11
  void add_drivedata(::std::string&& value);
  #endif
  void add_drivedata(const char* value);
  void add_drivedata(const void* value, size_t size);
  const ::google::protobuf::RepeatedPtrField< ::std::string>& drivedata() const;
  ::google::protobuf::RepeatedPtrField< ::std::string>* mutable_drivedata();

  // optional string row = 1 [default = ""];
  bool has_row() const;
  void clear_row();
  static const int kRowFieldNumber = 1;
  const ::std::string& row() const;
  void set_row(const ::std::string& value);
  #if LANG_CXX11
  void set_row(::std::string&& value);
  #endif
  void set_row(const char* value);
  void set_row(const char* value, size_t size);
  ::std::string* mutable_row();
  ::std::string* release_row();
  void set_allocated_row(::std::string* row);

  // optional string column = 2 [default = ""];
  bool has_column() const;
  void clear_column();
  static const int kColumnFieldNumber = 2;
  const ::std::string& column() const;
  void set_column(const ::std::string& value);
  #if LANG_CXX11
  void set_column(::std::string&& value);
  #endif
  void set_column(const char* value);
  void set_column(const char* value, size_t size);
  ::std::string* mutable_column();
  ::std::string* release_column();
  void set_allocated_column(::std::string* column);

  // optional string command = 3 [default = ""];
  bool has_command() const;
  void clear_command();
  static const int kCommandFieldNumber = 3;
  const ::std::string& command() const;
  void set_command(const ::std::string& value);
  #if LANG_CXX11
  void set_command(::std::string&& value);
  #endif
  void set_command(const char* value);
  void set_command(const char* value, size_t size);
  ::std::string* mutable_command();
  ::std::string* release_command();
  void set_allocated_command(::std::string* command);

  // optional bytes data = 4 [default = ""];
  bool has_data() const;
  void clear_data();
  static const int kDataFieldNumber = 4;
  const ::std::string& data() const;
  void set_data(const ::std::string& value);
  #if LANG_CXX11
  void set_data(::std::string&& value);
  #endif
  void set_data(const char* value);
  void set_data(const void* value, size_t size);
  ::std::string* mutable_data();
  ::std::string* release_data();
  void set_allocated_data(::std::string* data);

  // optional string status = 5 [default = ""];
  bool has_status() const;
  void clear_status();
  static const int kStatusFieldNumber = 5;
  const ::std::string& status() const;
  void set_status(const ::std::string& value);
  #if LANG_CXX11
  void set_status(::std::string&& value);
  #endif
  void set_status(const char* value);
  void set_status(const char* value, size_t size);
  ::std::string* mutable_status();
  ::std::string* release_status();
  void set_allocated_status(::std::string* status);

  // optional string status_code = 6 [default = ""];
  bool has_status_code() const;
  void clear_status_code();
  static const int kStatusCodeFieldNumber = 6;
  const ::std::string& status_code() const;
  void set_status_code(const ::std::string& value);
  #if LANG_CXX11
  void set_status_code(::std::string&& value);
  #endif
  void set_status_code(const char* value);
  void set_status_code(const char* value, size_t size);
  ::std::string* mutable_status_code();
  ::std::string* release_status_code();
  void set_allocated_status_code(::std::string* status_code);

  // optional string sender = 8 [default = ""];
  bool has_sender() const;
  void clear_sender();
  static const int kSenderFieldNumber = 8;
  const ::std::string& sender() const;
  void set_sender(const ::std::string& value);
  #if LANG_CXX11
  void set_sender(::std::string&& value);
  #endif
  void set_sender(const char* value);
  void set_sender(const char* value, size_t size);
  ::std::string* mutable_sender();
  ::std::string* release_sender();
  void set_allocated_sender(::std::string* sender);

  // optional string receiver = 9 [default = ""];
  bool has_receiver() const;
  void clear_receiver();
  static const int kReceiverFieldNumber = 9;
  const ::std::string& receiver() const;
  void set_receiver(const ::std::string& value);
  #if LANG_CXX11
  void set_receiver(::std::string&& value);
  #endif
  void set_receiver(const char* value);
  void set_receiver(const char* value, size_t size);
  ::std::string* mutable_receiver();
  ::std::string* release_receiver();
  void set_allocated_receiver(::std::string* receiver);

  // @@protoc_insertion_point(class_scope:Packet)
 private:
  void set_has_row();
  void clear_has_row();
  void set_has_column();
  void clear_has_column();
  void set_has_command();
  void clear_has_command();
  void set_has_data();
  void clear_has_data();
  void set_has_status();
  void clear_has_status();
  void set_has_status_code();
  void clear_has_status_code();
  void set_has_sender();
  void clear_has_sender();
  void set_has_receiver();
  void clear_has_receiver();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable ::google::protobuf::internal::CachedSize _cached_size_;
  ::google::protobuf::RepeatedPtrField< ::std::string> arg_;
  ::google::protobuf::RepeatedPtrField< ::std::string> drivedata_;
  ::google::protobuf::internal::ArenaStringPtr row_;
  ::google::protobuf::internal::ArenaStringPtr column_;
  ::google::protobuf::internal::ArenaStringPtr command_;
  ::google::protobuf::internal::ArenaStringPtr data_;
  ::google::protobuf::internal::ArenaStringPtr status_;
  ::google::protobuf::internal::ArenaStringPtr status_code_;
  ::google::protobuf::internal::ArenaStringPtr sender_;
  ::google::protobuf::internal::ArenaStringPtr receiver_;
  friend struct ::protobuf_DATA_5fPACKET_2eproto::TableStruct;
};
// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// Packet

// optional string row = 1 [default = ""];
inline bool Packet::has_row() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Packet::set_has_row() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Packet::clear_has_row() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Packet::clear_row() {
  row_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_row();
}
inline const ::std::string& Packet::row() const {
  // @@protoc_insertion_point(field_get:Packet.row)
  return row_.GetNoArena();
}
inline void Packet::set_row(const ::std::string& value) {
  set_has_row();
  row_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.row)
}
#if LANG_CXX11
inline void Packet::set_row(::std::string&& value) {
  set_has_row();
  row_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.row)
}
#endif
inline void Packet::set_row(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_row();
  row_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.row)
}
inline void Packet::set_row(const char* value, size_t size) {
  set_has_row();
  row_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.row)
}
inline ::std::string* Packet::mutable_row() {
  set_has_row();
  // @@protoc_insertion_point(field_mutable:Packet.row)
  return row_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_row() {
  // @@protoc_insertion_point(field_release:Packet.row)
  if (!has_row()) {
    return NULL;
  }
  clear_has_row();
  return row_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_row(::std::string* row) {
  if (row != NULL) {
    set_has_row();
  } else {
    clear_has_row();
  }
  row_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), row);
  // @@protoc_insertion_point(field_set_allocated:Packet.row)
}

// optional string column = 2 [default = ""];
inline bool Packet::has_column() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void Packet::set_has_column() {
  _has_bits_[0] |= 0x00000002u;
}
inline void Packet::clear_has_column() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void Packet::clear_column() {
  column_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_column();
}
inline const ::std::string& Packet::column() const {
  // @@protoc_insertion_point(field_get:Packet.column)
  return column_.GetNoArena();
}
inline void Packet::set_column(const ::std::string& value) {
  set_has_column();
  column_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.column)
}
#if LANG_CXX11
inline void Packet::set_column(::std::string&& value) {
  set_has_column();
  column_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.column)
}
#endif
inline void Packet::set_column(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_column();
  column_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.column)
}
inline void Packet::set_column(const char* value, size_t size) {
  set_has_column();
  column_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.column)
}
inline ::std::string* Packet::mutable_column() {
  set_has_column();
  // @@protoc_insertion_point(field_mutable:Packet.column)
  return column_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_column() {
  // @@protoc_insertion_point(field_release:Packet.column)
  if (!has_column()) {
    return NULL;
  }
  clear_has_column();
  return column_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_column(::std::string* column) {
  if (column != NULL) {
    set_has_column();
  } else {
    clear_has_column();
  }
  column_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), column);
  // @@protoc_insertion_point(field_set_allocated:Packet.column)
}

// optional string command = 3 [default = ""];
inline bool Packet::has_command() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Packet::set_has_command() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Packet::clear_has_command() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Packet::clear_command() {
  command_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_command();
}
inline const ::std::string& Packet::command() const {
  // @@protoc_insertion_point(field_get:Packet.command)
  return command_.GetNoArena();
}
inline void Packet::set_command(const ::std::string& value) {
  set_has_command();
  command_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.command)
}
#if LANG_CXX11
inline void Packet::set_command(::std::string&& value) {
  set_has_command();
  command_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.command)
}
#endif
inline void Packet::set_command(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_command();
  command_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.command)
}
inline void Packet::set_command(const char* value, size_t size) {
  set_has_command();
  command_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.command)
}
inline ::std::string* Packet::mutable_command() {
  set_has_command();
  // @@protoc_insertion_point(field_mutable:Packet.command)
  return command_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_command() {
  // @@protoc_insertion_point(field_release:Packet.command)
  if (!has_command()) {
    return NULL;
  }
  clear_has_command();
  return command_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_command(::std::string* command) {
  if (command != NULL) {
    set_has_command();
  } else {
    clear_has_command();
  }
  command_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), command);
  // @@protoc_insertion_point(field_set_allocated:Packet.command)
}

// optional bytes data = 4 [default = ""];
inline bool Packet::has_data() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void Packet::set_has_data() {
  _has_bits_[0] |= 0x00000008u;
}
inline void Packet::clear_has_data() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void Packet::clear_data() {
  data_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_data();
}
inline const ::std::string& Packet::data() const {
  // @@protoc_insertion_point(field_get:Packet.data)
  return data_.GetNoArena();
}
inline void Packet::set_data(const ::std::string& value) {
  set_has_data();
  data_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.data)
}
#if LANG_CXX11
inline void Packet::set_data(::std::string&& value) {
  set_has_data();
  data_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.data)
}
#endif
inline void Packet::set_data(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_data();
  data_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.data)
}
inline void Packet::set_data(const void* value, size_t size) {
  set_has_data();
  data_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.data)
}
inline ::std::string* Packet::mutable_data() {
  set_has_data();
  // @@protoc_insertion_point(field_mutable:Packet.data)
  return data_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_data() {
  // @@protoc_insertion_point(field_release:Packet.data)
  if (!has_data()) {
    return NULL;
  }
  clear_has_data();
  return data_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_data(::std::string* data) {
  if (data != NULL) {
    set_has_data();
  } else {
    clear_has_data();
  }
  data_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), data);
  // @@protoc_insertion_point(field_set_allocated:Packet.data)
}

// optional string status = 5 [default = ""];
inline bool Packet::has_status() const {
  return (_has_bits_[0] & 0x00000010u) != 0;
}
inline void Packet::set_has_status() {
  _has_bits_[0] |= 0x00000010u;
}
inline void Packet::clear_has_status() {
  _has_bits_[0] &= ~0x00000010u;
}
inline void Packet::clear_status() {
  status_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_status();
}
inline const ::std::string& Packet::status() const {
  // @@protoc_insertion_point(field_get:Packet.status)
  return status_.GetNoArena();
}
inline void Packet::set_status(const ::std::string& value) {
  set_has_status();
  status_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.status)
}
#if LANG_CXX11
inline void Packet::set_status(::std::string&& value) {
  set_has_status();
  status_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.status)
}
#endif
inline void Packet::set_status(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_status();
  status_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.status)
}
inline void Packet::set_status(const char* value, size_t size) {
  set_has_status();
  status_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.status)
}
inline ::std::string* Packet::mutable_status() {
  set_has_status();
  // @@protoc_insertion_point(field_mutable:Packet.status)
  return status_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_status() {
  // @@protoc_insertion_point(field_release:Packet.status)
  if (!has_status()) {
    return NULL;
  }
  clear_has_status();
  return status_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_status(::std::string* status) {
  if (status != NULL) {
    set_has_status();
  } else {
    clear_has_status();
  }
  status_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), status);
  // @@protoc_insertion_point(field_set_allocated:Packet.status)
}

// optional string status_code = 6 [default = ""];
inline bool Packet::has_status_code() const {
  return (_has_bits_[0] & 0x00000020u) != 0;
}
inline void Packet::set_has_status_code() {
  _has_bits_[0] |= 0x00000020u;
}
inline void Packet::clear_has_status_code() {
  _has_bits_[0] &= ~0x00000020u;
}
inline void Packet::clear_status_code() {
  status_code_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_status_code();
}
inline const ::std::string& Packet::status_code() const {
  // @@protoc_insertion_point(field_get:Packet.status_code)
  return status_code_.GetNoArena();
}
inline void Packet::set_status_code(const ::std::string& value) {
  set_has_status_code();
  status_code_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.status_code)
}
#if LANG_CXX11
inline void Packet::set_status_code(::std::string&& value) {
  set_has_status_code();
  status_code_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.status_code)
}
#endif
inline void Packet::set_status_code(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_status_code();
  status_code_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.status_code)
}
inline void Packet::set_status_code(const char* value, size_t size) {
  set_has_status_code();
  status_code_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.status_code)
}
inline ::std::string* Packet::mutable_status_code() {
  set_has_status_code();
  // @@protoc_insertion_point(field_mutable:Packet.status_code)
  return status_code_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_status_code() {
  // @@protoc_insertion_point(field_release:Packet.status_code)
  if (!has_status_code()) {
    return NULL;
  }
  clear_has_status_code();
  return status_code_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_status_code(::std::string* status_code) {
  if (status_code != NULL) {
    set_has_status_code();
  } else {
    clear_has_status_code();
  }
  status_code_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), status_code);
  // @@protoc_insertion_point(field_set_allocated:Packet.status_code)
}

// repeated string arg = 7;
inline int Packet::arg_size() const {
  return arg_.size();
}
inline void Packet::clear_arg() {
  arg_.Clear();
}
inline const ::std::string& Packet::arg(int index) const {
  // @@protoc_insertion_point(field_get:Packet.arg)
  return arg_.Get(index);
}
inline ::std::string* Packet::mutable_arg(int index) {
  // @@protoc_insertion_point(field_mutable:Packet.arg)
  return arg_.Mutable(index);
}
inline void Packet::set_arg(int index, const ::std::string& value) {
  // @@protoc_insertion_point(field_set:Packet.arg)
  arg_.Mutable(index)->assign(value);
}
#if LANG_CXX11
inline void Packet::set_arg(int index, ::std::string&& value) {
  // @@protoc_insertion_point(field_set:Packet.arg)
  arg_.Mutable(index)->assign(std::move(value));
}
#endif
inline void Packet::set_arg(int index, const char* value) {
  GOOGLE_DCHECK(value != NULL);
  arg_.Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Packet.arg)
}
inline void Packet::set_arg(int index, const char* value, size_t size) {
  arg_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Packet.arg)
}
inline ::std::string* Packet::add_arg() {
  // @@protoc_insertion_point(field_add_mutable:Packet.arg)
  return arg_.Add();
}
inline void Packet::add_arg(const ::std::string& value) {
  arg_.Add()->assign(value);
  // @@protoc_insertion_point(field_add:Packet.arg)
}
#if LANG_CXX11
inline void Packet::add_arg(::std::string&& value) {
  arg_.Add(std::move(value));
  // @@protoc_insertion_point(field_add:Packet.arg)
}
#endif
inline void Packet::add_arg(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  arg_.Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Packet.arg)
}
inline void Packet::add_arg(const char* value, size_t size) {
  arg_.Add()->assign(reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Packet.arg)
}
inline const ::google::protobuf::RepeatedPtrField< ::std::string>&
Packet::arg() const {
  // @@protoc_insertion_point(field_list:Packet.arg)
  return arg_;
}
inline ::google::protobuf::RepeatedPtrField< ::std::string>*
Packet::mutable_arg() {
  // @@protoc_insertion_point(field_mutable_list:Packet.arg)
  return &arg_;
}

// optional string sender = 8 [default = ""];
inline bool Packet::has_sender() const {
  return (_has_bits_[0] & 0x00000040u) != 0;
}
inline void Packet::set_has_sender() {
  _has_bits_[0] |= 0x00000040u;
}
inline void Packet::clear_has_sender() {
  _has_bits_[0] &= ~0x00000040u;
}
inline void Packet::clear_sender() {
  sender_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_sender();
}
inline const ::std::string& Packet::sender() const {
  // @@protoc_insertion_point(field_get:Packet.sender)
  return sender_.GetNoArena();
}
inline void Packet::set_sender(const ::std::string& value) {
  set_has_sender();
  sender_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.sender)
}
#if LANG_CXX11
inline void Packet::set_sender(::std::string&& value) {
  set_has_sender();
  sender_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.sender)
}
#endif
inline void Packet::set_sender(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_sender();
  sender_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.sender)
}
inline void Packet::set_sender(const char* value, size_t size) {
  set_has_sender();
  sender_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.sender)
}
inline ::std::string* Packet::mutable_sender() {
  set_has_sender();
  // @@protoc_insertion_point(field_mutable:Packet.sender)
  return sender_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_sender() {
  // @@protoc_insertion_point(field_release:Packet.sender)
  if (!has_sender()) {
    return NULL;
  }
  clear_has_sender();
  return sender_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_sender(::std::string* sender) {
  if (sender != NULL) {
    set_has_sender();
  } else {
    clear_has_sender();
  }
  sender_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), sender);
  // @@protoc_insertion_point(field_set_allocated:Packet.sender)
}

// optional string receiver = 9 [default = ""];
inline bool Packet::has_receiver() const {
  return (_has_bits_[0] & 0x00000080u) != 0;
}
inline void Packet::set_has_receiver() {
  _has_bits_[0] |= 0x00000080u;
}
inline void Packet::clear_has_receiver() {
  _has_bits_[0] &= ~0x00000080u;
}
inline void Packet::clear_receiver() {
  receiver_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_receiver();
}
inline const ::std::string& Packet::receiver() const {
  // @@protoc_insertion_point(field_get:Packet.receiver)
  return receiver_.GetNoArena();
}
inline void Packet::set_receiver(const ::std::string& value) {
  set_has_receiver();
  receiver_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:Packet.receiver)
}
#if LANG_CXX11
inline void Packet::set_receiver(::std::string&& value) {
  set_has_receiver();
  receiver_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:Packet.receiver)
}
#endif
inline void Packet::set_receiver(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  set_has_receiver();
  receiver_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:Packet.receiver)
}
inline void Packet::set_receiver(const char* value, size_t size) {
  set_has_receiver();
  receiver_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:Packet.receiver)
}
inline ::std::string* Packet::mutable_receiver() {
  set_has_receiver();
  // @@protoc_insertion_point(field_mutable:Packet.receiver)
  return receiver_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* Packet::release_receiver() {
  // @@protoc_insertion_point(field_release:Packet.receiver)
  if (!has_receiver()) {
    return NULL;
  }
  clear_has_receiver();
  return receiver_.ReleaseNonDefaultNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void Packet::set_allocated_receiver(::std::string* receiver) {
  if (receiver != NULL) {
    set_has_receiver();
  } else {
    clear_has_receiver();
  }
  receiver_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), receiver);
  // @@protoc_insertion_point(field_set_allocated:Packet.receiver)
}

// repeated bytes drivedata = 10;
inline int Packet::drivedata_size() const {
  return drivedata_.size();
}
inline void Packet::clear_drivedata() {
  drivedata_.Clear();
}
inline const ::std::string& Packet::drivedata(int index) const {
  // @@protoc_insertion_point(field_get:Packet.drivedata)
  return drivedata_.Get(index);
}
inline ::std::string* Packet::mutable_drivedata(int index) {
  // @@protoc_insertion_point(field_mutable:Packet.drivedata)
  return drivedata_.Mutable(index);
}
inline void Packet::set_drivedata(int index, const ::std::string& value) {
  // @@protoc_insertion_point(field_set:Packet.drivedata)
  drivedata_.Mutable(index)->assign(value);
}
#if LANG_CXX11
inline void Packet::set_drivedata(int index, ::std::string&& value) {
  // @@protoc_insertion_point(field_set:Packet.drivedata)
  drivedata_.Mutable(index)->assign(std::move(value));
}
#endif
inline void Packet::set_drivedata(int index, const char* value) {
  GOOGLE_DCHECK(value != NULL);
  drivedata_.Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Packet.drivedata)
}
inline void Packet::set_drivedata(int index, const void* value, size_t size) {
  drivedata_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Packet.drivedata)
}
inline ::std::string* Packet::add_drivedata() {
  // @@protoc_insertion_point(field_add_mutable:Packet.drivedata)
  return drivedata_.Add();
}
inline void Packet::add_drivedata(const ::std::string& value) {
  drivedata_.Add()->assign(value);
  // @@protoc_insertion_point(field_add:Packet.drivedata)
}
#if LANG_CXX11
inline void Packet::add_drivedata(::std::string&& value) {
  drivedata_.Add(std::move(value));
  // @@protoc_insertion_point(field_add:Packet.drivedata)
}
#endif
inline void Packet::add_drivedata(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  drivedata_.Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Packet.drivedata)
}
inline void Packet::add_drivedata(const void* value, size_t size) {
  drivedata_.Add()->assign(reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Packet.drivedata)
}
inline const ::google::protobuf::RepeatedPtrField< ::std::string>&
Packet::drivedata() const {
  // @@protoc_insertion_point(field_list:Packet.drivedata)
  return drivedata_;
}
inline ::google::protobuf::RepeatedPtrField< ::std::string>*
Packet::mutable_drivedata() {
  // @@protoc_insertion_point(field_mutable_list:Packet.drivedata)
  return &drivedata_;
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_INCLUDED_DATA_5fPACKET_2eproto
