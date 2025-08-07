// Copyright (c) 2021-present Sparky Studios. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Codec.h>

FlacCodec::FlacDecoderInternal::~FlacDecoderInternal()
{
    if (_decoder_handle)
    {
        FLAC__stream_decoder_delete(_decoder_handle);
        _decoder_handle = nullptr;
    }
}

void FlacCodec::FlacDecoderInternal::set_current_output_buffer(AudioBuffer* output, AmUInt64 size, AmUInt64 offset)
{
    _current_output_buffer = output;
    _current_output_buffer_size = size;
    _current_output_buffer_offset = offset;
}

::FLAC__StreamDecoderWriteStatus FlacCodec::FlacDecoderInternal::write_callback(
    const FLAC__StreamDecoder* decoder, const ::FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (self->_current_output_buffer == nullptr)
        return ::FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    AmUInt64 sample = self->_read_frame_count % self->_current_output_buffer_size;

    self->_need_more_frames = true;

    for (AmUInt32 i = 0; i < frame->header.blocksize; i++)
    {
        for (AmUInt32 j = 0; j < frame->header.channels; j++)
        {
            auto& channel = self->_current_output_buffer->GetChannel(j);
            channel[sample + self->_current_output_buffer_offset] = AmInt16ToReal32(buffer[j][i]);
        }

        sample++;

        if (sample >= self->_current_output_buffer_size)
        {
            self->_need_more_frames = false;
            break;
        }
    }

    self->_read_frame_count = sample;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacCodec::FlacDecoderInternal::metadata_callback(
    const FLAC__StreamDecoder* decoder, const ::FLAC__StreamMetadata* metadata, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
        auto total_samples = metadata->data.stream_info.total_samples;
        auto sample_rate = metadata->data.stream_info.sample_rate;
        auto channels = metadata->data.stream_info.channels;
        auto bps = metadata->data.stream_info.bits_per_sample;
        auto frame_size = metadata->data.stream_info.min_framesize;

        self->_decoder->m_format.SetAll(
            sample_rate, channels, bps, total_samples, channels * sizeof(AmAudioSample), eAudioSampleFormat_Float32);
    }
}

void FlacCodec::FlacDecoderInternal::error_callback(
    const FLAC__StreamDecoder* decoder, ::FLAC__StreamDecoderErrorStatus status, void* client_data)
{
    amLogError("Got error callback: %s", FLAC__StreamDecoderErrorStatusString[status]);
}

::FLAC__StreamDecoderReadStatus FlacCodec::FlacDecoderInternal::read_callback(
    const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (self->_decoder->_file == nullptr)
        return ::FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    *bytes = self->_decoder->_file->Read(buffer, *bytes);
    return self->_decoder->_file->Eof() ? ::FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : ::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

::FLAC__StreamDecoderSeekStatus FlacCodec::FlacDecoderInternal::seek_callback(
    const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (self->_decoder->_file == nullptr)
        return ::FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

    self->_decoder->_file->Seek(absolute_byte_offset, eFileSeekOrigin_Start);
    return ::FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

::FLAC__StreamDecoderTellStatus FlacCodec::FlacDecoderInternal::tell_callback(
    const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (self->_decoder->_file == nullptr)
        return ::FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

    *absolute_byte_offset = self->_decoder->_file->Position();
    return ::FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

::FLAC__StreamDecoderLengthStatus FlacCodec::FlacDecoderInternal::length_callback(
    const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    if (self->_decoder->_file == nullptr)
        return ::FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    *stream_length = self->_decoder->_file->Length();
    return ::FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool FlacCodec::FlacDecoderInternal::eof_callback(const FLAC__StreamDecoder* decoder, void* client_data)
{
    auto* self = static_cast<FlacDecoderInternal*>(client_data);

    return self->_decoder->_file != nullptr ? self->_decoder->_file->Eof() : true;
}

bool FlacCodec::FlacDecoderInternal::init()
{
    FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
        _decoder_handle, read_callback, seek_callback, tell_callback, length_callback, eof_callback, write_callback, metadata_callback,
        error_callback, this);

    return init_status == FLAC__STREAM_DECODER_INIT_STATUS_OK;
}

bool FlacCodec::FlacDecoderInternal::finish()
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_finish(_decoder_handle);
}

bool FlacCodec::FlacDecoderInternal::set_md5_checking(bool value)
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_set_md5_checking(_decoder_handle, value);
}

bool FlacCodec::FlacDecoderInternal::set_metadata_respond(FLAC__MetadataType type)
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_set_metadata_respond(_decoder_handle, type);
}

bool FlacCodec::FlacDecoderInternal::process_until_end_of_metadata()
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_process_until_end_of_metadata(_decoder_handle);
}

bool FlacCodec::FlacDecoderInternal::process_until_end_of_stream()
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_process_until_end_of_stream(_decoder_handle);
}

bool FlacCodec::FlacDecoderInternal::process_single()
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_process_single(_decoder_handle);
}

bool FlacCodec::FlacDecoderInternal::seek_absolute(FLAC__uint64 sample)
{
    return (!_decoder_handle) ? false : FLAC__stream_decoder_seek_absolute(_decoder_handle, sample);
}

FLAC__StreamDecoderState FlacCodec::FlacDecoderInternal::get_state() const
{
    return (!_decoder_handle) ? FLAC__STREAM_DECODER_UNINITIALIZED : FLAC__stream_decoder_get_state(_decoder_handle);
}

FlacCodec::FlacEncoderInternal::~FlacEncoderInternal()
{
    if (_encoder_handle)
    {
        FLAC__stream_encoder_delete(_encoder_handle);
        _encoder_handle = nullptr;
    }
}

void FlacCodec::FlacEncoderInternal::set_current_input_buffer(const AudioBuffer* input, AmUInt64 size, AmUInt64 offset)
{
    _current_input_buffer = input;
    _current_input_buffer_size = size;
    _current_input_buffer_offset = offset;
}

::FLAC__StreamEncoderWriteStatus FlacCodec::FlacEncoderInternal::write_callback(
    const FLAC__StreamEncoder* encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void* client_data)
{
    auto* self = static_cast<FlacEncoderInternal*>(client_data);

    if (self->_encoder->_file == nullptr)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

    size_t bytes_written = self->_encoder->_file->Write(buffer, bytes);
    if (bytes_written != bytes)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

    self->_written_samples += samples;
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

::FLAC__StreamEncoderSeekStatus FlacCodec::FlacEncoderInternal::seek_callback(
    const FLAC__StreamEncoder* encoder, FLAC__uint64 absolute_byte_offset, void* client_data)
{
    auto* self = static_cast<FlacEncoderInternal*>(client_data);

    if (self->_encoder->_file == nullptr)
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

    self->_encoder->_file->Seek(absolute_byte_offset, eFileSeekOrigin_Start);
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

::FLAC__StreamEncoderTellStatus FlacCodec::FlacEncoderInternal::tell_callback(
    const FLAC__StreamEncoder* encoder, FLAC__uint64* absolute_byte_offset, void* client_data)
{
    auto* self = static_cast<FlacEncoderInternal*>(client_data);

    if (self->_encoder->_file == nullptr)
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

    *absolute_byte_offset = self->_encoder->_file->Position();
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

void FlacCodec::FlacEncoderInternal::metadata_callback(
    const FLAC__StreamEncoder* encoder, const ::FLAC__StreamMetadata* metadata, void* client_data)
{
    // Metadata callback for encoder - typically not much to do here
    // Could be used for progress reporting or custom metadata handling
}

bool FlacCodec::FlacEncoderInternal::init()
{
    FLAC__StreamEncoderInitStatus init_status =
        FLAC__stream_encoder_init_stream(_encoder_handle, write_callback, seek_callback, tell_callback, metadata_callback, this);

    return init_status == FLAC__STREAM_ENCODER_INIT_STATUS_OK;
}

bool FlacCodec::FlacEncoderInternal::finish()
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_finish(_encoder_handle);
}

bool FlacCodec::FlacEncoderInternal::set_compression_level(unsigned level)
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_set_compression_level(_encoder_handle, level);
}

bool FlacCodec::FlacEncoderInternal::set_channels(unsigned channels)
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_set_channels(_encoder_handle, channels);
}

bool FlacCodec::FlacEncoderInternal::set_bits_per_sample(unsigned bps)
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_set_bits_per_sample(_encoder_handle, bps);
}

bool FlacCodec::FlacEncoderInternal::set_sample_rate(unsigned sample_rate)
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_set_sample_rate(_encoder_handle, sample_rate);
}

bool FlacCodec::FlacEncoderInternal::set_total_samples_estimate(FLAC__uint64 total_samples)
{
    _total_samples_estimate = total_samples;
    return (!_encoder_handle) ? false : FLAC__stream_encoder_set_total_samples_estimate(_encoder_handle, total_samples);
}

bool FlacCodec::FlacEncoderInternal::process_interleaved(const FLAC__int32 buffer[], unsigned samples)
{
    return (!_encoder_handle) ? false : FLAC__stream_encoder_process_interleaved(_encoder_handle, buffer, samples);
}

FLAC__StreamEncoderState FlacCodec::FlacEncoderInternal::get_state() const
{
    return (!_encoder_handle) ? FLAC__STREAM_ENCODER_UNINITIALIZED : FLAC__stream_encoder_get_state(_encoder_handle);
}

FlacCodec::FlacCodec()
    : Codec("flac")
{}

bool FlacCodec::FlacDecoder::Open(std::shared_ptr<File> file)
{
    _file = file;

    _flac.set_md5_checking(true);
    _flac.set_metadata_respond(::FLAC__METADATA_TYPE_STREAMINFO);

    if (!_flac.init())
    {
        _file.reset();
        amLogError("Failed to initialize FLAC decoder");
        return false;
    }

    if (!_flac.process_until_end_of_metadata())
    {
        _file.reset();
        amLogError("Unable to read metadata for FLAC file: " AM_OS_CHAR_FMT, file->GetPath().c_str());
        return false;
    }

    _initialized = true;

    return true;
}

bool FlacCodec::FlacDecoder::Close()
{
    if (_initialized)
    {
        _flac.finish();
        _file.reset();

        m_format = SoundFormat();
        _initialized = false;
    }

    // true because it is already closed
    return true;
}

AmUInt64 FlacCodec::FlacDecoder::Load(AudioBuffer* out)
{
    if (!_initialized)
        return 0;

    _flac.set_current_output_buffer(out, m_format.GetFramesCount(), 0);

    if (!Seek(0))
        return 0;

    if (_flac.process_until_end_of_stream())
        return _flac.read_frame_count();

    return 0;
}

AmUInt64 FlacCodec::FlacDecoder::Stream(AudioBuffer* out, AmUInt64 bufferOffset, AmUInt64 seekOffset, AmUInt64 length)
{
    if (!_initialized)
        return 0;

    _flac.set_current_output_buffer(out, length, bufferOffset);

    bool seeked = Seek(seekOffset);

    while (_flac.need_more_frames() && _flac.get_state() != FLAC__STREAM_DECODER_END_OF_STREAM)
        _flac.process_single();

    AmUInt64 read = _flac.read_frame_count();
    _flac.reset_read_frame_count();

    return read;
}

bool FlacCodec::FlacDecoder::Seek(AmUInt64 offset)
{
    return _flac.seek_absolute(offset);
}

bool FlacCodec::FlacEncoder::Open(std::shared_ptr<File> file)
{
    _file = file;

    if (m_format.GetNumChannels() != 1 && m_format.GetNumChannels() != 2)
    {
        _file.reset();
        amLogError("Unsupported number of channels for FLAC encoding");
        return false;
    }

    // Set encoder parameters before initialization
    if (!_flac.set_compression_level(5)) // Medium compression
    {
        _file.reset();
        amLogError("Failed to set FLAC compression level");
        return false;
    }

    if (!_flac.set_channels(m_format.GetNumChannels()) || !_flac.set_bits_per_sample(m_format.GetBitsPerSample()) ||
        !_flac.set_sample_rate(m_format.GetSampleRate()))
    {
        _file.reset();
        amLogError("Failed to set FLAC encoder parameters");
        return false;
    }

    if (!_flac.init())
    {
        _file.reset();
        amLogError("Failed to initialize FLAC encoder");
        return false;
    }

    _initialized = true;
    return true;
}

bool FlacCodec::FlacEncoder::Close()
{
    if (_initialized)
    {
        _flac.finish();
        _file.reset();
        _initialized = false;
    }

    return true;
}

AmUInt64 FlacCodec::FlacEncoder::Write(AudioBuffer* in, AmUInt64 offset, AmUInt64 length)
{
    if (!_initialized || !in)
        return 0;

    AmUInt32 channels = m_format.GetNumChannels();
    if (channels != in->GetChannelCount())
    {
        amLogError("Invalid number of channels for FLAC encoder. Expected %u channels, got %zu channels", channels, in->GetChannelCount());
        return 0;
    }

    std::vector<FLAC__int32> flac_buffer;
    flac_buffer.reserve(length * channels);

    for (AmUInt64 frame = 0; frame < length; frame++)
    {
        for (AmUInt16 channel = 0; channel < channels; channel++)
        {
            AmReal32 sample = (*in)[channel][offset + frame];
            AmInt16 sampleInt16 = AmReal32ToInt16(sample);

            flac_buffer.push_back(static_cast<FLAC__int32>(sampleInt16));
        }
    }

    if (_flac.process_interleaved(flac_buffer.data(), static_cast<unsigned>(length)))
        return length;

    return 0;
}

std::shared_ptr<Codec::Decoder> FlacCodec::CreateDecoder()
{
    return AmSharedPtr<FlacDecoder, eMemoryPoolKind_Codec>::Make(this);
}

std::shared_ptr<Codec::Encoder> FlacCodec::CreateEncoder()
{
    return AmSharedPtr<FlacEncoder, eMemoryPoolKind_Codec>::Make(this);
}

bool FlacCodec::CanHandleFile(std::shared_ptr<File> file) const
{
    const auto& path = file->GetPath();
    return path.find(AM_OS_STRING(".flac")) != AmOsString::npos;
}
