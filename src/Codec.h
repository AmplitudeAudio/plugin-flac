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

#pragma once

#ifndef _AM_PLUGIN_CODEC_FLAC_CODEC_H
#define _AM_PLUGIN_CODEC_FLAC_CODEC_H

#include <Plugin.h>

#include <FLAC/all.h>

class FlacCodec final : public Codec
{
public:
    class FlacDecoder;
    class FlacDecoderInternal;
    class FlacEncoder;
    class FlacEncoderInternal;

    class FlacDecoderInternal final
    {
    public:
        explicit FlacDecoderInternal(FlacDecoder* decoder)
            : _decoder(decoder)
            , _decoder_handle(nullptr)
            , _need_more_frames(false)
            , _current_output_buffer(nullptr)
            , _read_frame_count(0)
            , _current_output_buffer_size(0)
            , _current_output_buffer_offset(0)
        {
            _decoder_handle = FLAC__stream_decoder_new();
        }

        ~FlacDecoderInternal();

        FlacDecoderInternal(const FlacDecoderInternal&) = delete;
        FlacDecoderInternal& operator=(const FlacDecoderInternal&) = delete;

        bool init();
        bool finish();
        bool set_md5_checking(bool value);
        bool set_metadata_respond(FLAC__MetadataType type);
        bool process_until_end_of_metadata();
        bool process_until_end_of_stream();
        bool process_single();
        bool seek_absolute(FLAC__uint64 sample);
        FLAC__StreamDecoderState get_state() const;

        void set_current_output_buffer(AudioBuffer* output, AmUInt64 size, AmUInt64 offset);

        [[nodiscard]] bool need_more_frames() const
        {
            return _need_more_frames;
        }

        [[nodiscard]] AmUInt64 read_frame_count() const
        {
            return _read_frame_count;
        }

        void reset_read_frame_count()
        {
            _read_frame_count = 0;
        }

        // Static callback functions for C API
        static ::FLAC__StreamDecoderWriteStatus write_callback(
            const FLAC__StreamDecoder* decoder, const ::FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
        static void metadata_callback(const FLAC__StreamDecoder* decoder, const ::FLAC__StreamMetadata* metadata, void* client_data);
        static void error_callback(const FLAC__StreamDecoder* decoder, ::FLAC__StreamDecoderErrorStatus status, void* client_data);
        static ::FLAC__StreamDecoderReadStatus read_callback(
            const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data);
        static ::FLAC__StreamDecoderSeekStatus seek_callback(
            const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data);
        static ::FLAC__StreamDecoderTellStatus tell_callback(
            const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data);
        static ::FLAC__StreamDecoderLengthStatus length_callback(
            const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data);
        static FLAC__bool eof_callback(const FLAC__StreamDecoder* decoder, void* client_data);

    private:
        FlacDecoder* _decoder;
        FLAC__StreamDecoder* _decoder_handle;

        bool _need_more_frames;
        AmUInt64 _read_frame_count;

        AudioBuffer* _current_output_buffer;
        AmUInt64 _current_output_buffer_size;
        AmUInt64 _current_output_buffer_offset;
    };

    class FlacEncoderInternal final
    {
    public:
        explicit FlacEncoderInternal(FlacEncoder* encoder)
            : _encoder(encoder)
            , _encoder_handle(nullptr)
            , _current_input_buffer(nullptr)
            , _current_input_buffer_size(0)
            , _current_input_buffer_offset(0)
            , _total_samples_estimate(0)
            , _written_samples(0)
        {
            _encoder_handle = FLAC__stream_encoder_new();
        }

        ~FlacEncoderInternal();

        FlacEncoderInternal(const FlacEncoderInternal&) = delete;
        FlacEncoderInternal& operator=(const FlacEncoderInternal&) = delete;

        bool init();
        bool finish();
        bool set_compression_level(unsigned level);
        bool set_channels(unsigned channels);
        bool set_bits_per_sample(unsigned bps);
        bool set_sample_rate(unsigned sample_rate);
        bool set_total_samples_estimate(FLAC__uint64 total_samples);
        bool process_interleaved(const FLAC__int32 buffer[], unsigned samples);
        FLAC__StreamEncoderState get_state() const;

        void set_current_input_buffer(const AudioBuffer* input, AmUInt64 size, AmUInt64 offset);

        [[nodiscard]] AmUInt64 written_samples() const
        {
            return _written_samples;
        }

        void reset_written_samples()
        {
            _written_samples = 0;
        }

        // Static callback functions for C API
        static ::FLAC__StreamEncoderWriteStatus write_callback(
            const FLAC__StreamEncoder* encoder,
            const FLAC__byte buffer[],
            size_t bytes,
            unsigned samples,
            unsigned current_frame,
            void* client_data);
        static ::FLAC__StreamEncoderSeekStatus seek_callback(
            const FLAC__StreamEncoder* encoder, FLAC__uint64 absolute_byte_offset, void* client_data);
        static ::FLAC__StreamEncoderTellStatus tell_callback(
            const FLAC__StreamEncoder* encoder, FLAC__uint64* absolute_byte_offset, void* client_data);
        static void metadata_callback(const FLAC__StreamEncoder* encoder, const ::FLAC__StreamMetadata* metadata, void* client_data);

    private:
        FlacEncoder* _encoder;
        FLAC__StreamEncoder* _encoder_handle;

        const AudioBuffer* _current_input_buffer;
        AmUInt64 _current_input_buffer_size;
        AmUInt64 _current_input_buffer_offset;
        FLAC__uint64 _total_samples_estimate;
        AmUInt64 _written_samples;
    };

    class FlacDecoder final : public Codec::Decoder
    {
    public:
        explicit FlacDecoder(const Codec* codec)
            : Codec::Decoder(codec)
            , _initialized(false)
            , _flac(this)
        {}

        bool Open(std::shared_ptr<File> file) override;

        bool Close() override;

        AmUInt64 Load(AudioBuffer* out) override;

        AmUInt64 Stream(AudioBuffer* out, AmUInt64 bufferOffset, AmUInt64 seekOffset, AmUInt64 length) override;

        bool Seek(AmUInt64 offset) override;

    private:
        friend class FlacDecoderInternal;

        bool _initialized;

        std::shared_ptr<File> _file;
        FlacDecoderInternal _flac;
    };

    class FlacEncoder final : public Codec::Encoder
    {
    public:
        explicit FlacEncoder(const Codec* codec)
            : Codec::Encoder(codec)
            , _initialized(false)
            , _flac(this)
        {}

        bool Open(std::shared_ptr<File> file) override;

        bool Close() override;

        AmUInt64 Write(AudioBuffer* in, AmUInt64 offset, AmUInt64 length) override;

    private:
        friend class FlacEncoderInternal;

        bool _initialized;

        std::shared_ptr<File> _file;
        FlacEncoderInternal _flac;
    };

    FlacCodec();

    ~FlacCodec() override = default;

    [[nodiscard]] std::shared_ptr<Decoder> CreateDecoder() override;

    [[nodiscard]] std::shared_ptr<Encoder> CreateEncoder() override;

    [[nodiscard]] bool CanHandleFile(std::shared_ptr<File> file) const override;
};
#endif // _AM_PLUGIN_CODEC_FLAC_CODEC_H
