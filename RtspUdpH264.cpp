#pragma region RTSPUDPH264
////////////////////////////////////////////////////////////////////////////////

DWORD
RTSPUDPH264::
GetFOURCC() const
{
    return MAKEFOURCC('H','2','6','4');
}

const string &
RTSPUDPH264::
GetMimeSubtypeName() const
{
    static const string name("H264");
    return name;
}

///
/// Parse line containing SDP fmtp attribute for H.264.
///
/// @post config is non-empty if returns true, empty if false.
///
/// @param[in] line Line containing the fmtp attribute.
/// @param[out] config Configuration bytes.
/// @return Whether the fmtp attribute was parsed.
bool
RTSPUDPH264::
ParseFmtp(const string &line, vector<BYTE> &config) const
{
    bool parsed = false;

    config.clear();

    // Make sure we have an fmtp line to parse.
    if (!line.empty())
    {
        vector<string> fmtp_parts;
        string trimmed = trim_copy_if(line, is_any_of(" ;"));
        split(fmtp_parts, trimmed, is_any_of(" ;"), token_compress_on);
        string modeString;
        string sets;
        BOOST_FOREACH(const string &parameter, fmtp_parts)
        {
            if (starts_with(parameter, "packetization-mode"))
            {
                string::size_type iIndex = parameter.find("=");
                if (iIndex != string::npos)
                {
                    iIndex++; // move past '='
                    modeString = &parameter[iIndex];
                }
            }
            else if (starts_with(parameter, "sprop-parameter-sets"))
            {
                string::size_type iIndex = parameter.find("=");
                if (iIndex != string::npos)
                {
                    iIndex++; // move past '='
                    sets = &parameter[iIndex];
                }
            }
        }

        parsed = SupportedPacketizationMode(modeString) &&
            ParseSpropParameterSets(sets, config);
    }

    return parsed;
}

bool
RTSPUDPH264::
ParseConfig(const vector<BYTE> &bytes, int &width, int &height,
    double &frameRate) const
{
    ibitstream bin(const_cast<BYTE *>(&bytes[0]), bytes.size() * CHAR_BIT);

    static const BYTE zero_byte = 0x00;
    static const bitset<24> start_code_prefix_one_3bytes = 0x000001;
    static const bitset<1> forbidden_zero_bit = 0;
    bitset<2> nal_ref_idc;
    bitset<5> nal_unit_type;
    BYTE profile_idc;
    bool constraint_set_flag[6];
    static const bitset<2> reserved_zero_2bits = 0;
    BYTE level_idc;
    UE_V seq_parameter_set_id;
    UE_V chroma_format_idc;
    bool separate_colour_plane_flag;
    UE_V bit_depth_luma_minus8;
    UE_V bit_depth_chroma_minus8;
    bool qpprime_y_zero_transform_bypass_flag;
    bool seq_scaling_matrix_present_flag;
    bool seq_scaling_list_present_flag; // Should be array, but we don't care about value
    UE_V log2_max_frame_num_minus4;
    UE_V pic_order_cnt_type;
    UE_V log2_max_pic_order_cnt_lsb_minus4;
    bool delta_pic_order_always_zero_flag;
    SE_V offset_for_non_ref_pic;
    SE_V offset_for_top_to_bottom_field;
    UE_V num_ref_frames_in_pic_order_cnt_cycle;
    SE_V offset_for_ref_frame; // Should be array, but we don't care about value
    UE_V max_num_ref_frames;
    bool gaps_in_frame_num_value_allowed_flag;
    UE_V pic_width_in_mbs_minus1;
    UE_V pic_height_in_map_units_minus1;
    bool frame_mbs_only_flag;

    bin >> zero_byte
        >> start_code_prefix_one_3bytes
        >> forbidden_zero_bit
        >> nal_ref_idc
        >> nal_unit_type;
    bin >> profile_idc;
    bin >> constraint_set_flag[0]
        >> constraint_set_flag[1]
        >> constraint_set_flag[2]
        >> constraint_set_flag[3]
        >> constraint_set_flag[4]
        >> constraint_set_flag[5];
    bin >> reserved_zero_2bits;
    bin >> level_idc;
    ParseExpGolombCode(bin, seq_parameter_set_id);
    switch (profile_idc)
    {
    // NOTE: I have noticed that profile values are added to this check over
    // time as the H.264 standard is updated. If you are having decode
    // problems, be sure to check the latest version of the spec to see if
    // further values have been added.
    case 100: // 0x64 - High
    case 110: // 0x6E - High 10
    case 122: // 0x7A - High 4:2:2
    case 244: // 0xF4 - High 4:4:4 Predictive
    case 44: // 0x2C - CAVLC 4:4:4 Intra
    case 83: // 0x53 - Scalable Baseline
    case 86: // 0x56 - Scalable High
    case 118: // 0x76 - Multiview High
    case 128: // 0x80 - Stereo High
        ParseExpGolombCode(bin, chroma_format_idc);
        if (chroma_format_idc == 3)
        {
            bin >> separate_colour_plane_flag;
        }
        ParseExpGolombCode(bin, bit_depth_luma_minus8);
        ParseExpGolombCode(bin, bit_depth_chroma_minus8);
        bin >> qpprime_y_zero_transform_bypass_flag
            >> seq_scaling_matrix_present_flag;
        if (seq_scaling_matrix_present_flag)
        {
            for (size_t i = 0; i < (chroma_format_idc != 3 ? 8u : 12u); ++i)
            {
                bin >> seq_scaling_list_present_flag;
                if (seq_scaling_list_present_flag)
                {
                    unsigned lastScale = 8;
                    unsigned nextScale = 8;
                    for (size_t j = 0; j < (i < 6 ? 16u : 64u); ++j)
                    {
                        if (nextScale != 0)
                        {
                            SE_V delta_scale;
                            ParseExpGolombCode(bin, delta_scale);
                            nextScale = (lastScale + delta_scale + 256) % 256;
                            if (nextScale != 0)
                            {
                                lastScale = nextScale;
                            }
                        }
                    }
                }
            }
        }
        break;

    case 66: // 0x42 - Baseline and Constrained Baseline
    case 77: // 0x4D - Main
    case 88: // 0x58 - Extended
    default:
        // Do nothing.
        break;
    }

    ParseExpGolombCode(bin, log2_max_frame_num_minus4);
    ParseExpGolombCode(bin, pic_order_cnt_type);
    switch (pic_order_cnt_type)
    {
    case 0:
        ParseExpGolombCode(bin, log2_max_pic_order_cnt_lsb_minus4);
        break;

    case 1:
        bin >> delta_pic_order_always_zero_flag;
        ParseExpGolombCode(bin, offset_for_non_ref_pic);
        ParseExpGolombCode(bin, offset_for_top_to_bottom_field);
        ParseExpGolombCode(bin, num_ref_frames_in_pic_order_cnt_cycle);
        for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
        {
            // (We overwrite this variable with each iteration because we
            // don't plan on actually using the value.)
            ParseExpGolombCode(bin, offset_for_ref_frame);
        }
        break;

    case 2:
        // Do nothing.
        break;

    default:
        // From ITU-T H.264 Recommendation: "The value of pic_order_cnt_type
        // shall be in the range of 0 to 2, inclusive." Use ibitstream's state
        // to record this semantic error.
        bin.setstate(std::ios_base::failbit);
        break;
    }
    ParseExpGolombCode(bin, max_num_ref_frames);
    bin >> gaps_in_frame_num_value_allowed_flag;
    ParseExpGolombCode(bin, pic_width_in_mbs_minus1);
    ParseExpGolombCode(bin, pic_height_in_map_units_minus1);
    bin >> frame_mbs_only_flag;
    // We don't need to parse any further now that we have all the info we
    // need to calculate the width and height...

    width = (pic_width_in_mbs_minus1 + 1) * 16;
    height = (pic_height_in_map_units_minus1 + 1) * 16 * (2 - frame_mbs_only_flag);
    // We don't care about the fields that follow...

    return bin.good();
}

void
RTSPUDPH264::
SaveInBandParameterSet(const vector<BYTE> &set)
{
    assert(!set.empty());

    // Assure there will be room for this, new parameter set.
    while (m_inBandParameterSets.size() >= MAXIMUM_IN_BAND_PARAMETER_SETS)
    {
        m_inBandParameterSets.pop_front();
    }

    m_inBandParameterSets.push_back(set);

    assert(!m_inBandParameterSets.empty());
    assert(m_inBandParameterSets.size() <= MAXIMUM_IN_BAND_PARAMETER_SETS);
}

void
RTSPUDPH264::
AppendInBandParameterSets(vector<BYTE> &frame)
{
    assert(!m_inBandParameterSets.empty());

    BOOST_FOREACH(const vector<BYTE> &set, m_inBandParameterSets)
    {
        AppendNalUnitPrefix(frame);

        frame.insert(frame.end(), set.begin(), set.end());
    }

    m_inBandParameterSets.clear();

    assert(m_inBandParameterSets.empty());
    assert(!frame.empty());
}

void
RTSPUDPH264::
ExtractFrame(RTPPacket *packet, bool marker, const vector<BYTE> &configBytes,
    vector<BYTE> &frame, bool &fullFrame, bool &keyFrame)
{
    assert(packet != NULL);

    BYTE *payload = packet->GetPayloadData();
    size_t payloadLength = packet->GetPayloadLength();

    static const bitset<1> forbidden_zero_bit = 0;
    bitset<2> nal_ref_idc;
    bitset<5> fragmentation_unit_type;
    ibitstream bin(payload, payloadLength * CHAR_BIT);
    bin >> forbidden_zero_bit >> nal_ref_idc >> fragmentation_unit_type;

    // Ignore all packets where NRI, or nal_ref_idc, is 0.
    //
    // We primarily do this because the SEI packets from some cameras cause
    // the UMC H.264 decoder we use to throw an exception. This is safe to do
    // because, from RFC3984: "nal_ref_idc. ... a value of 00 indicates that
    // the content of the NAL unit is not used to reconstruct reference
    // pictures for inter picture prediction. Such NAL units can be discarded
    // without risking the integrity of the reference pictures." and
    // "Intelligent receivers having to discard packets or NALUs should first
    // discard all packets/NALUs in which the value of the NRI field of the NAL
    // unit type octet is equal to 0. This will minimize the impact on user
    // experience and keep the reference pictures intact."
    if (nal_ref_idc.to_ulong() != 0)
    {
        switch (fragmentation_unit_type.to_ulong())
        {
        case NAL_UT_FU_A:
        {
            bool start_fragment;
            bool end_fragment;
            bitset<1> reserved;
            bitset<5> nal_unit_type;
            bin >> start_fragment >> end_fragment >> reserved >> nal_unit_type;

            if (start_fragment || frame.empty())
            {
                // Should never have start bit set while already assembling
                // frame. Clear frame just to be sure.
                frame.clear();

                // Append any picture or sequence parameter sets received since
                // previous frame.
                if (!m_inBandParameterSets.empty())
                {
                    AppendInBandParameterSets(frame);
                }

                // NRI + NAL type tells decoder type of NAL unit.
                payload[1] = static_cast<BYTE>(
                    (nal_ref_idc.to_ulong() << 5) | nal_unit_type.to_ulong());

                AppendNalUnitPrefix(frame);

                // Don't include FU-indicator byte; not part of payload
                AppendPacket(packet, 1, frame);
            }
            else
            {
                // Append fragments without FU-indicator- and FU-header-bytes.
                AppendPacket(packet, 2, frame);
            }

            // (If end fragment, RTP marker bit should also be set. The former
            // is more reliable, though, according to RFC 3894.)
            if (end_fragment)
            {
                if (nal_unit_type.to_ulong() == NAL_UT_IDR_SLICE)
                {
                    // IDR frame; prime encoder(s) with SPS & PPS data.
                    frame.insert(frame.begin(), configBytes.begin(),
                        configBytes.end());
                    keyFrame = true;
                }

                fullFrame = true;
            }
            break;
        }
        case NAL_UT_STAP_A:
            // TBD - Supposed to support for packetization-mode=1. Ignore for
            // now (we haven't encountered from any cameras).
            break;

        case NAL_UT_STAP_B:
        case NAL_UT_MTAP16:
        case NAL_UT_MTAP24:
        case NAL_UT_FU_B:
            // Not allowed for packetization-mode=1 (non-interleaved), which
            // is all we support.
            break;

        case NAL_UT_SPS:
        case NAL_UT_PPS:
            // Save parameter set (without RTP header) for subsequent inclusion
            // with next frame.
            SaveInBandParameterSet(vector<BYTE>(packet->GetPayloadData(),
                packet->GetPayloadData() + packet->GetPayloadLength()));
            break;

        case NAL_UT_IDR_SLICE:
            // Any frame made up of fragmentation units should have been
            // completed by now. IOW, the frame vector should now be empty.
            // Just in case it isn't, however, clear it.
            frame.clear();

            // IDR frame; prime encoder(s) with SPS & PPS data.
            frame.insert(frame.begin(), configBytes.begin(), configBytes.end());

            // Append any picture or sequence parameter sets received since
            // previous frame.
            if (!m_inBandParameterSets.empty())
            {
                AppendInBandParameterSets(frame);
            }

            AppendNalUnitPrefix(frame);

            // Append entire payload (no FU header bytes to ignore).
            AppendPacket(packet, 0, frame);

            keyFrame = true; // An IDR slice is inherently a key frame.

            fullFrame = true; // And this is a full frame, too.
            break;

        default:
            // A non-fragmentation-unit should never appear with fragmentation
            // units. We could assert(frame.empty()). Instead, to gracefully
            // handle this error, just make sure we're not appending this NAL
            // unit to others.
            frame.clear();

            // Append any picture or sequence parameter sets received since
            // previous frame.
            if (!m_inBandParameterSets.empty())
            {
                AppendInBandParameterSets(frame);
            }

            // NOTE: Could a non-FU-A ever be a keyframe? If so, should we prepend
            // SPS & PPS? Maybe we never encounter these packets in practice.

            AppendNalUnitPrefix(frame);

            AppendPacket(packet, 0, frame);

            fullFrame = true;

            break;
        }
    }
}

bool
RTSPUDPH264::
EndOfFrame(RTPPacket *packet) const
{
    assert(packet != NULL);

    bool end = false;

    // For H.264, we need to wade into the payload to determine end of frame.
    // (The RTP marker bit is also set, just like MPEG4; however, the standard
    // for RTP H.264 packetization, RFC3984, says, don't rely on that.)
    static const bitset<1> forbidden_zero_bit = 0;
    bitset<2> nal_ref_idc;
    ibitstream bin(packet->GetPayloadData(),
        packet->GetPayloadLength() * CHAR_BIT);
    bin >> forbidden_zero_bit >> nal_ref_idc;

    if (nal_ref_idc.to_ulong() != 0)
    {
        bitset<5> fragmentation_unit_type;
        bin >> fragmentation_unit_type;
        switch (fragmentation_unit_type.to_ulong())
        {
        case NAL_UT_FU_A:
        // (We don't support B, but end bit applies to this type, too.)
        case NAL_UT_FU_B:
            bool start;
            bin >> start >> end;
            break;

        default:
            // Do nothing.
            break;
        }
    }

    return end;
}

bool
RTSPUDPH264::
ConstructMediaSample(const BYTE *Begin, const BYTE *End, bool keyFrame,
    const vector<BYTE> &configBytes, const RTSPSource &source,
    bool &got_keyframe, CComPtr<IMediaSample> &sample) const
{
    bool constructed = false;

    assert(Begin != NULL);
    assert(End != NULL);
    assert(sample != NULL);

    if (Begin != NULL && End != NULL)
    {
        size_t frameSize = End - Begin;

        static const bitset<1> forbidden_zero_bit = 0;
        bitset<2> nal_ref_idc;
        bitset<5> nal_unit_type;
        ibitstream bin(Begin, frameSize * CHAR_BIT);
        for (size_t i = 0; i < arraysize(NAL_UNIT_PREFIX); ++i)
        {
            bin >> NAL_UNIT_PREFIX[i];
        }
        bin >> forbidden_zero_bit >> nal_ref_idc >> nal_unit_type;
        if (bin.good())
        {
            BYTE *buf;
            if (SUCCEEDED(sample->GetPointer(&buf)))
            {
                assert(sample->GetSize() > frameSize);

                memcpy(buf, Begin, static_cast<int> (frameSize));
                sample->SetActualDataLength(static_cast<int> (frameSize));
                sample->SetSyncPoint(keyFrame ? TRUE : FALSE);

                constructed = true;
            }
        }
    }

    return constructed;
}

#pragma endregion
