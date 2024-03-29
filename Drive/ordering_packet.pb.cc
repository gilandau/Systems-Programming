// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: ordering_packet.proto

#include "ordering_packet.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/port.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// This is a temporary google only hack
#ifdef GOOGLE_PROTOBUF_ENFORCE_UNIQUENESS
#include "third_party/protobuf/version.h"
#endif
// @@protoc_insertion_point(includes)

class QueuepacketDefaultTypeInternal {
 public:
  ::google::protobuf::internal::ExplicitlyConstructed<Queuepacket>
      _instance;
} _Queuepacket_default_instance_;
namespace protobuf_ordering_5fpacket_2eproto {
static void InitDefaultsQueuepacket() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  {
    void* ptr = &::_Queuepacket_default_instance_;
    new (ptr) ::Queuepacket();
    ::google::protobuf::internal::OnShutdownDestroyMessage(ptr);
  }
  ::Queuepacket::InitAsDefaultInstance();
}

::google::protobuf::internal::SCCInfo<0> scc_info_Queuepacket =
    {{ATOMIC_VAR_INIT(::google::protobuf::internal::SCCInfoBase::kUninitialized), 0, InitDefaultsQueuepacket}, {}};

void InitDefaults() {
  ::google::protobuf::internal::InitSCC(&scc_info_Queuepacket.base);
}

::google::protobuf::Metadata file_level_metadata[1];

const ::google::protobuf::uint32 TableStruct::offsets[] GOOGLE_PROTOBUF_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, _has_bits_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, command_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, row_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, column_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, data_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, data_to_compare_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, msgid_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, version_number_),
  GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(::Queuepacket, sender_socket_),
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
};
static const ::google::protobuf::internal::MigrationSchema schemas[] GOOGLE_PROTOBUF_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
  { 0, 13, sizeof(::Queuepacket)},
};

static ::google::protobuf::Message const * const file_default_instances[] = {
  reinterpret_cast<const ::google::protobuf::Message*>(&::_Queuepacket_default_instance_),
};

void protobuf_AssignDescriptors() {
  AddDescriptors();
  AssignDescriptors(
      "ordering_packet.proto", schemas, file_default_instances, TableStruct::offsets,
      file_level_metadata, NULL, NULL);
}

void protobuf_AssignDescriptorsOnce() {
  static ::google::protobuf::internal::once_flag once;
  ::google::protobuf::internal::call_once(once, protobuf_AssignDescriptors);
}

void protobuf_RegisterTypes(const ::std::string&) GOOGLE_PROTOBUF_ATTRIBUTE_COLD;
void protobuf_RegisterTypes(const ::std::string&) {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::internal::RegisterAllTypes(file_level_metadata, 1);
}

void AddDescriptorsImpl() {
  InitDefaults();
  static const char descriptor[] GOOGLE_PROTOBUF_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
      "\n\025ordering_packet.proto\"\262\001\n\013Queuepacket\022"
      "\021\n\007command\030\001 \001(\t:\000\022\r\n\003row\030\002 \001(\t:\000\022\020\n\006col"
      "umn\030\003 \001(\t:\000\022\016\n\004data\030\004 \001(\t:\000\022\031\n\017data_to_c"
      "ompare\030\005 \001(\t:\000\022\017\n\005msgid\030\006 \001(\t:\000\022\031\n\016versi"
      "on_number\030\007 \001(\005:\0010\022\030\n\rsender_socket\030\010 \001("
      "\005:\0010"
  };
  ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
      descriptor, 204);
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(
    "ordering_packet.proto", &protobuf_RegisterTypes);
}

void AddDescriptors() {
  static ::google::protobuf::internal::once_flag once;
  ::google::protobuf::internal::call_once(once, AddDescriptorsImpl);
}
// Force AddDescriptors() to be called at dynamic initialization time.
struct StaticDescriptorInitializer {
  StaticDescriptorInitializer() {
    AddDescriptors();
  }
} static_descriptor_initializer;
}  // namespace protobuf_ordering_5fpacket_2eproto

// ===================================================================

void Queuepacket::InitAsDefaultInstance() {
}
#if !defined(_MSC_VER) || _MSC_VER >= 1900
const int Queuepacket::kCommandFieldNumber;
const int Queuepacket::kRowFieldNumber;
const int Queuepacket::kColumnFieldNumber;
const int Queuepacket::kDataFieldNumber;
const int Queuepacket::kDataToCompareFieldNumber;
const int Queuepacket::kMsgidFieldNumber;
const int Queuepacket::kVersionNumberFieldNumber;
const int Queuepacket::kSenderSocketFieldNumber;
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1900

Queuepacket::Queuepacket()
  : ::google::protobuf::Message(), _internal_metadata_(NULL) {
  ::google::protobuf::internal::InitSCC(
      &protobuf_ordering_5fpacket_2eproto::scc_info_Queuepacket.base);
  SharedCtor();
  // @@protoc_insertion_point(constructor:Queuepacket)
}
Queuepacket::Queuepacket(const Queuepacket& from)
  : ::google::protobuf::Message(),
      _internal_metadata_(NULL),
      _has_bits_(from._has_bits_) {
  _internal_metadata_.MergeFrom(from._internal_metadata_);
  command_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_command()) {
    command_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.command_);
  }
  row_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_row()) {
    row_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.row_);
  }
  column_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_column()) {
    column_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.column_);
  }
  data_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_data()) {
    data_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.data_);
  }
  data_to_compare_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_data_to_compare()) {
    data_to_compare_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.data_to_compare_);
  }
  msgid_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  if (from.has_msgid()) {
    msgid_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.msgid_);
  }
  ::memcpy(&version_number_, &from.version_number_,
    static_cast<size_t>(reinterpret_cast<char*>(&sender_socket_) -
    reinterpret_cast<char*>(&version_number_)) + sizeof(sender_socket_));
  // @@protoc_insertion_point(copy_constructor:Queuepacket)
}

void Queuepacket::SharedCtor() {
  command_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  row_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  column_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  data_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  data_to_compare_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  msgid_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  ::memset(&version_number_, 0, static_cast<size_t>(
      reinterpret_cast<char*>(&sender_socket_) -
      reinterpret_cast<char*>(&version_number_)) + sizeof(sender_socket_));
}

Queuepacket::~Queuepacket() {
  // @@protoc_insertion_point(destructor:Queuepacket)
  SharedDtor();
}

void Queuepacket::SharedDtor() {
  command_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  row_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  column_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  data_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  data_to_compare_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  msgid_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}

void Queuepacket::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}
const ::google::protobuf::Descriptor* Queuepacket::descriptor() {
  ::protobuf_ordering_5fpacket_2eproto::protobuf_AssignDescriptorsOnce();
  return ::protobuf_ordering_5fpacket_2eproto::file_level_metadata[kIndexInFileMessages].descriptor;
}

const Queuepacket& Queuepacket::default_instance() {
  ::google::protobuf::internal::InitSCC(&protobuf_ordering_5fpacket_2eproto::scc_info_Queuepacket.base);
  return *internal_default_instance();
}


void Queuepacket::Clear() {
// @@protoc_insertion_point(message_clear_start:Queuepacket)
  ::google::protobuf::uint32 cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  if (cached_has_bits & 63u) {
    if (cached_has_bits & 0x00000001u) {
      command_.ClearNonDefaultToEmptyNoArena();
    }
    if (cached_has_bits & 0x00000002u) {
      row_.ClearNonDefaultToEmptyNoArena();
    }
    if (cached_has_bits & 0x00000004u) {
      column_.ClearNonDefaultToEmptyNoArena();
    }
    if (cached_has_bits & 0x00000008u) {
      data_.ClearNonDefaultToEmptyNoArena();
    }
    if (cached_has_bits & 0x00000010u) {
      data_to_compare_.ClearNonDefaultToEmptyNoArena();
    }
    if (cached_has_bits & 0x00000020u) {
      msgid_.ClearNonDefaultToEmptyNoArena();
    }
  }
  if (cached_has_bits & 192u) {
    ::memset(&version_number_, 0, static_cast<size_t>(
        reinterpret_cast<char*>(&sender_socket_) -
        reinterpret_cast<char*>(&version_number_)) + sizeof(sender_socket_));
  }
  _has_bits_.Clear();
  _internal_metadata_.Clear();
}

bool Queuepacket::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!GOOGLE_PREDICT_TRUE(EXPRESSION)) goto failure
  ::google::protobuf::uint32 tag;
  // @@protoc_insertion_point(parse_start:Queuepacket)
  for (;;) {
    ::std::pair<::google::protobuf::uint32, bool> p = input->ReadTagWithCutoffNoLastTag(127u);
    tag = p.first;
    if (!p.second) goto handle_unusual;
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // optional string command = 1 [default = ""];
      case 1: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(10u /* 10 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_command()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->command().data(), static_cast<int>(this->command().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.command");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional string row = 2 [default = ""];
      case 2: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(18u /* 18 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_row()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->row().data(), static_cast<int>(this->row().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.row");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional string column = 3 [default = ""];
      case 3: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(26u /* 26 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_column()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->column().data(), static_cast<int>(this->column().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.column");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional string data = 4 [default = ""];
      case 4: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(34u /* 34 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_data()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->data().data(), static_cast<int>(this->data().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.data");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional string data_to_compare = 5 [default = ""];
      case 5: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(42u /* 42 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_data_to_compare()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->data_to_compare().data(), static_cast<int>(this->data_to_compare().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.data_to_compare");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional string msgid = 6 [default = ""];
      case 6: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(50u /* 50 & 0xFF */)) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->mutable_msgid()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
            this->msgid().data(), static_cast<int>(this->msgid().length()),
            ::google::protobuf::internal::WireFormat::PARSE,
            "Queuepacket.msgid");
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional int32 version_number = 7 [default = 0];
      case 7: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(56u /* 56 & 0xFF */)) {
          set_has_version_number();
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::int32, ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(
                 input, &version_number_)));
        } else {
          goto handle_unusual;
        }
        break;
      }

      // optional int32 sender_socket = 8 [default = 0];
      case 8: {
        if (static_cast< ::google::protobuf::uint8>(tag) ==
            static_cast< ::google::protobuf::uint8>(64u /* 64 & 0xFF */)) {
          set_has_sender_socket();
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::int32, ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(
                 input, &sender_socket_)));
        } else {
          goto handle_unusual;
        }
        break;
      }

      default: {
      handle_unusual:
        if (tag == 0) {
          goto success;
        }
        DO_(::google::protobuf::internal::WireFormat::SkipField(
              input, tag, _internal_metadata_.mutable_unknown_fields()));
        break;
      }
    }
  }
success:
  // @@protoc_insertion_point(parse_success:Queuepacket)
  return true;
failure:
  // @@protoc_insertion_point(parse_failure:Queuepacket)
  return false;
#undef DO_
}

void Queuepacket::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // @@protoc_insertion_point(serialize_start:Queuepacket)
  ::google::protobuf::uint32 cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  // optional string command = 1 [default = ""];
  if (cached_has_bits & 0x00000001u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->command().data(), static_cast<int>(this->command().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.command");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      1, this->command(), output);
  }

  // optional string row = 2 [default = ""];
  if (cached_has_bits & 0x00000002u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->row().data(), static_cast<int>(this->row().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.row");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      2, this->row(), output);
  }

  // optional string column = 3 [default = ""];
  if (cached_has_bits & 0x00000004u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->column().data(), static_cast<int>(this->column().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.column");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      3, this->column(), output);
  }

  // optional string data = 4 [default = ""];
  if (cached_has_bits & 0x00000008u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->data().data(), static_cast<int>(this->data().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.data");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      4, this->data(), output);
  }

  // optional string data_to_compare = 5 [default = ""];
  if (cached_has_bits & 0x00000010u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->data_to_compare().data(), static_cast<int>(this->data_to_compare().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.data_to_compare");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      5, this->data_to_compare(), output);
  }

  // optional string msgid = 6 [default = ""];
  if (cached_has_bits & 0x00000020u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->msgid().data(), static_cast<int>(this->msgid().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.msgid");
    ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
      6, this->msgid(), output);
  }

  // optional int32 version_number = 7 [default = 0];
  if (cached_has_bits & 0x00000040u) {
    ::google::protobuf::internal::WireFormatLite::WriteInt32(7, this->version_number(), output);
  }

  // optional int32 sender_socket = 8 [default = 0];
  if (cached_has_bits & 0x00000080u) {
    ::google::protobuf::internal::WireFormatLite::WriteInt32(8, this->sender_socket(), output);
  }

  if (_internal_metadata_.have_unknown_fields()) {
    ::google::protobuf::internal::WireFormat::SerializeUnknownFields(
        _internal_metadata_.unknown_fields(), output);
  }
  // @@protoc_insertion_point(serialize_end:Queuepacket)
}

::google::protobuf::uint8* Queuepacket::InternalSerializeWithCachedSizesToArray(
    bool deterministic, ::google::protobuf::uint8* target) const {
  (void)deterministic; // Unused
  // @@protoc_insertion_point(serialize_to_array_start:Queuepacket)
  ::google::protobuf::uint32 cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  // optional string command = 1 [default = ""];
  if (cached_has_bits & 0x00000001u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->command().data(), static_cast<int>(this->command().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.command");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        1, this->command(), target);
  }

  // optional string row = 2 [default = ""];
  if (cached_has_bits & 0x00000002u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->row().data(), static_cast<int>(this->row().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.row");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        2, this->row(), target);
  }

  // optional string column = 3 [default = ""];
  if (cached_has_bits & 0x00000004u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->column().data(), static_cast<int>(this->column().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.column");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        3, this->column(), target);
  }

  // optional string data = 4 [default = ""];
  if (cached_has_bits & 0x00000008u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->data().data(), static_cast<int>(this->data().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.data");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        4, this->data(), target);
  }

  // optional string data_to_compare = 5 [default = ""];
  if (cached_has_bits & 0x00000010u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->data_to_compare().data(), static_cast<int>(this->data_to_compare().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.data_to_compare");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        5, this->data_to_compare(), target);
  }

  // optional string msgid = 6 [default = ""];
  if (cached_has_bits & 0x00000020u) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(
      this->msgid().data(), static_cast<int>(this->msgid().length()),
      ::google::protobuf::internal::WireFormat::SERIALIZE,
      "Queuepacket.msgid");
    target =
      ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
        6, this->msgid(), target);
  }

  // optional int32 version_number = 7 [default = 0];
  if (cached_has_bits & 0x00000040u) {
    target = ::google::protobuf::internal::WireFormatLite::WriteInt32ToArray(7, this->version_number(), target);
  }

  // optional int32 sender_socket = 8 [default = 0];
  if (cached_has_bits & 0x00000080u) {
    target = ::google::protobuf::internal::WireFormatLite::WriteInt32ToArray(8, this->sender_socket(), target);
  }

  if (_internal_metadata_.have_unknown_fields()) {
    target = ::google::protobuf::internal::WireFormat::SerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields(), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Queuepacket)
  return target;
}

size_t Queuepacket::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Queuepacket)
  size_t total_size = 0;

  if (_internal_metadata_.have_unknown_fields()) {
    total_size +=
      ::google::protobuf::internal::WireFormat::ComputeUnknownFieldsSize(
        _internal_metadata_.unknown_fields());
  }
  if (_has_bits_[0 / 32] & 255u) {
    // optional string command = 1 [default = ""];
    if (has_command()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->command());
    }

    // optional string row = 2 [default = ""];
    if (has_row()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->row());
    }

    // optional string column = 3 [default = ""];
    if (has_column()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->column());
    }

    // optional string data = 4 [default = ""];
    if (has_data()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->data());
    }

    // optional string data_to_compare = 5 [default = ""];
    if (has_data_to_compare()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->data_to_compare());
    }

    // optional string msgid = 6 [default = ""];
    if (has_msgid()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::StringSize(
          this->msgid());
    }

    // optional int32 version_number = 7 [default = 0];
    if (has_version_number()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::Int32Size(
          this->version_number());
    }

    // optional int32 sender_socket = 8 [default = 0];
    if (has_sender_socket()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::Int32Size(
          this->sender_socket());
    }

  }
  int cached_size = ::google::protobuf::internal::ToCachedSize(total_size);
  SetCachedSize(cached_size);
  return total_size;
}

void Queuepacket::MergeFrom(const ::google::protobuf::Message& from) {
// @@protoc_insertion_point(generalized_merge_from_start:Queuepacket)
  GOOGLE_DCHECK_NE(&from, this);
  const Queuepacket* source =
      ::google::protobuf::internal::DynamicCastToGenerated<const Queuepacket>(
          &from);
  if (source == NULL) {
  // @@protoc_insertion_point(generalized_merge_from_cast_fail:Queuepacket)
    ::google::protobuf::internal::ReflectionOps::Merge(from, this);
  } else {
  // @@protoc_insertion_point(generalized_merge_from_cast_success:Queuepacket)
    MergeFrom(*source);
  }
}

void Queuepacket::MergeFrom(const Queuepacket& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:Queuepacket)
  GOOGLE_DCHECK_NE(&from, this);
  _internal_metadata_.MergeFrom(from._internal_metadata_);
  ::google::protobuf::uint32 cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._has_bits_[0];
  if (cached_has_bits & 255u) {
    if (cached_has_bits & 0x00000001u) {
      set_has_command();
      command_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.command_);
    }
    if (cached_has_bits & 0x00000002u) {
      set_has_row();
      row_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.row_);
    }
    if (cached_has_bits & 0x00000004u) {
      set_has_column();
      column_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.column_);
    }
    if (cached_has_bits & 0x00000008u) {
      set_has_data();
      data_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.data_);
    }
    if (cached_has_bits & 0x00000010u) {
      set_has_data_to_compare();
      data_to_compare_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.data_to_compare_);
    }
    if (cached_has_bits & 0x00000020u) {
      set_has_msgid();
      msgid_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), from.msgid_);
    }
    if (cached_has_bits & 0x00000040u) {
      version_number_ = from.version_number_;
    }
    if (cached_has_bits & 0x00000080u) {
      sender_socket_ = from.sender_socket_;
    }
    _has_bits_[0] |= cached_has_bits;
  }
}

void Queuepacket::CopyFrom(const ::google::protobuf::Message& from) {
// @@protoc_insertion_point(generalized_copy_from_start:Queuepacket)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

void Queuepacket::CopyFrom(const Queuepacket& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Queuepacket)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool Queuepacket::IsInitialized() const {
  return true;
}

void Queuepacket::Swap(Queuepacket* other) {
  if (other == this) return;
  InternalSwap(other);
}
void Queuepacket::InternalSwap(Queuepacket* other) {
  using std::swap;
  command_.Swap(&other->command_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  row_.Swap(&other->row_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  column_.Swap(&other->column_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  data_.Swap(&other->data_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  data_to_compare_.Swap(&other->data_to_compare_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  msgid_.Swap(&other->msgid_, &::google::protobuf::internal::GetEmptyStringAlreadyInited(),
    GetArenaNoVirtual());
  swap(version_number_, other->version_number_);
  swap(sender_socket_, other->sender_socket_);
  swap(_has_bits_[0], other->_has_bits_[0]);
  _internal_metadata_.Swap(&other->_internal_metadata_);
}

::google::protobuf::Metadata Queuepacket::GetMetadata() const {
  protobuf_ordering_5fpacket_2eproto::protobuf_AssignDescriptorsOnce();
  return ::protobuf_ordering_5fpacket_2eproto::file_level_metadata[kIndexInFileMessages];
}


// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
template<> GOOGLE_PROTOBUF_ATTRIBUTE_NOINLINE ::Queuepacket* Arena::CreateMaybeMessage< ::Queuepacket >(Arena* arena) {
  return Arena::CreateInternal< ::Queuepacket >(arena);
}
}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)
