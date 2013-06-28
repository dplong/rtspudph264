///
/// Abstract class that represents a video encoding.
class RTSPUDPEncoding
{
public:
    ///
    /// Get FOURCC representing video format on this stream.
    ///
    /// @return FOURCC for video stream.
    virtual DWORD GetFOURCC() const = 0;

    ///
    /// Get MIME subtype for this encoding.
    ///
    /// @return MIME subtype.
    virtual const string &GetMimeSubtypeName() const = 0;

    ///
    /// Parse SDP parameters from reply to our RTSP DESCRIBE request.
    ///
    /// @post Returns whether SUCCEEDED(hr) is true.
    ///
    /// @param[in] encodingName Encoding name taken from the rtpmap attribute.
    /// @param[out] frameRate Video frame rate.
    /// @param[in] fmtpLine Line containing the fmtp attribute.
    /// @param[out] configBytes Sequence of encoded bytes from fmtp attribute.
    /// @param[out] width Frame width in pixels.
    /// @param[out] height Frame height in pixels.
    /// @param[out] hr Error code.
    /// @return Whether the rtpmap and fmtp attributes fmtp attributes were parsed.
    virtual bool ParseSdp(const string &encodingName, double &frameRate,
        const string &fmtpLine, vector<BYTE> &configBytes,
        int &width, int &height, HRESULT &hr) const;

    ///
    /// Determine whether this packet contains the last part of a frame.
    ///
    /// @param[in] packet RTP packet.
    /// @return Whether this is an end-of-frame packet.
    virtual bool EndOfFrame(RTPPacket *packet) const;

    ///
    /// Extract one or more partial frames with the same timestamp.
    ///
    /// @note A frame is composed of a sequence of parts with the same
    /// timestamp and is usually fragmented across mutiple RTP packets.
    ///
    /// @param[in] packet RTP packet.
    /// @param[in] marker Whether marker bit was set in RTP header (unused).
    /// @param[in] configBytes Configuration bytes from the SDP line, a=fmtp.
    /// @param[in,out] frame Video frame under construction.
    /// @param[out] fullFrame Whether the entire frame has been read.
    /// @param[out] keyFrame Whether this is a keyframe.
    virtual void ExtractFrame(RTPPacket *packet, bool marker,
        const vector<BYTE> &configBytes, vector<BYTE> &frame, bool &fullFrame,
        bool &keyFrame);

    ///
    /// Construct media sample containing compressed frame.
    ///
    /// @param[in] Begin Beginning of the compressed frame.
    /// @param[in] End One past the end of the compressed frame.
    /// @param[in] keyFrame Whether this is a keyframe.
    /// @param[in] configBytes Configuration bytes from the SDP line, a=fmtp.
    /// @param[in] source Reference back to containing RTSPSource.
    /// @param[in,out] got_keyframe Whether an keyframe has been encountered yet (unused).
    /// @param[out] sample Where sample is constructed.
    /// @return Whether the sample was constructed.
    virtual bool ConstructMediaSample(const BYTE *Begin, const BYTE *End,
        bool keyFrame, const vector<BYTE> &configBytes,
        const RTSPSource &source, bool &got_keyframe,
        CComPtr<IMediaSample> &sample) const = 0;

protected:
    ///
    /// Parse line containing SDP fmtp attribute.
    ///
    /// @post config is non-empty if returns true, empty if false.
    ///
    /// @param[in] line Line containing the fmtp attribute.
    /// @param[out] config Configuration bytes.
    /// @return Whether the fmtp attribute was parsed.
    virtual bool ParseFmtp(const string &line, vector<BYTE> &config) const = 0;

    ///
    /// Parse a config string from an RTSP header.
    ///
    /// @param bytes[in] Config bytes.
    /// @param width[out] Receives video width.
    /// @param height[out] Receives video height.
    /// @param frameRate[out] Frames per second.
    /// @return true if successful.
    virtual bool ParseConfig(const vector<BYTE> &bytes, int &width, int &height,
        double &frameRate) const = 0;
};

///
/// H.264-specific behavior.
class RTSPUDPH264 : public RTSPUDPEncoding
{
public:
    ///
    /// Get FOURCC representing video format on this stream.
    ///
    /// @return FOURCC for video stream.
    DWORD GetFOURCC() const;

    ///
    /// Get MIME subtype for this encoding.
    ///
    /// @return MIME subtype.
    const string &GetMimeSubtypeName() const;

    ///
    /// Determine whether this packet contains the last part of a frame.
    ///
    /// @param[in] packet RTP packet.
    /// @return Whether this is an end-of-frame packet.
    bool EndOfFrame(RTPPacket *packet) const;

    ///
    /// Extract one or more partial frames with the same timestamp.
    ///
    /// @note A frame is composed of a sequence of parts with the same
    /// timestamp and is usually fragmented across mutiple RTP packets.
    ///
    /// @param[in] packet RTP packet.
    /// @param[in] marker Whether marker bit was set in RTP header (unused).
    /// @param[in] configBytes Configuration bytes from the SDP line, a=fmtp.
    /// @param[in,out] frame Video frame under construction.
    /// @param[out] fullFrame Whether the entire frame has been read.
    /// @param[out] keyFrame Whether this is a keyframe.
    void ExtractFrame(RTPPacket *packet, bool marker,
        const vector<BYTE> &configBytes, vector<BYTE> &frame, bool &fullFrame,
        bool &keyFrame);

    ///
    /// Construct media sample containing compressed frame.
    ///
    /// @param[in] Begin Beginning of the compressed frame.
    /// @param[in] End One past the end of the compressed frame.
    /// @param[in] keyFrame Whether this is a keyframe.
    /// @param[in] configBytes Configuration bytes from the SDP line, a=fmtp.
    /// @param[in] source Reference back to containing RTSPSource.
    /// @param[in,out] got_keyframe Whether an keyframe has been encountered yet (unused).
    /// @param[out] sample Where sample is constructed.
    /// @return Whether the sample was constructed.
    bool ConstructMediaSample(const BYTE *Begin, const BYTE *End,
        bool keyFrame, const vector<BYTE> &configBytes,
        const RTSPSource &source, bool &got_keyframe,
        CComPtr<IMediaSample> &sample) const;

protected:
    ///
    /// Parse line containing SDP fmtp attribute.
    ///
    /// @post config is non-empty if returns true, empty if false.
    ///
    /// @param[in] line Line containing the fmtp attribute.
    /// @param[out] config Configuration bytes.
    /// @return Whether the fmtp attribute was parsed.
    bool ParseFmtp(const string &line, vector<BYTE> &config) const;

    ///
    /// Parse a config string from an RTSP header.
    ///
    /// @param bytes[in] Config bytes.
    /// @param width[out] Receives video width.
    /// @param height[out] Receives video height.
    /// @param frameRate[out] Frames per second. Not used.
    /// @return true if successful.
    bool ParseConfig(const vector<BYTE> &bytes, int &width, int &height,
        double &frameRate) const;

    ///
    /// Save picture/sequence parameter set.
    ///
    /// @pre set is not empty.
    /// @post m_inBandParameterSets is not empty.
    ///
    /// @param[in] set BYTE vector containing a picture or sequence parameter set.
    void SaveInBandParameterSet(const vector<BYTE> &set);

    ///
    /// Append picture and sequence parameter sets received since previous frame after prepending NAL-unit prefix to each.
    ///
    /// @pre m_inBandParameterSets is not empty.
    /// @post m_inBandParameterSets is empty; frame is not empty.
    ///
    /// @param[out] frame BYTE vector to which parameter sets are appended.
    void AppendInBandParameterSets(vector<BYTE> &frame);

    ///
    /// H.264 picture and sequence parameter sets received in-band.
    ///
    /// @note When we receive a sequence or picture parameter set in the actual
    /// H.264 video stream (as opposed to the RTSP DESCRIBE response), we add
    /// it to this list. When we receive the next frame, we remove all sets
    /// from the list, prepend the H.264 NAL-unit prefix to each set, and
    /// prepend the results to the frame before prepending the contents of
    /// m_config_bytes and passing it down the graph. IOW, this is just a very
    /// temnporary store for picture and sequence parameter sets.
    ///
    /// @note At one time, we passed each parameter set down the graph as a
    /// complete frame, but they aren't really "frames," and that screwed up
    /// playback of recorded video because each of them would occupy a slot in
    /// the AVI file where a video frame should be. To fix that, we just
    /// discarded all parameter sets, assuming they are redundant with the
    /// picture and sequence parameter sets the camera includes in the RTSP
    /// DESCRIBE request. However, we found one camera that relies on these
    /// in-band parameters sets, a Sony SNC-DF50N. Without them, the decoded
    /// video is black.
    ///
    /// @note Here are some sequence-parameter-set examples from a Sony
    /// SNC-DF50N:
    /// 6742801e95a02c0f6400
    /// 6742801e45680b03d900
    /// 6742801e65680b03d900
    /// 6742801e215a02c0f64000
    ///
    /// @note Here are some picture-parameter-set examples from a Sony
    /// SNC-DF50N:
    /// 68ce04f200
    /// 6848e0fc8000
    /// 686ce0fc8000
    /// 68210e0fc800
    list<vector<BYTE> > m_inBandParameterSets;

    ///
    /// Maximum number of picture/sequence parameter sets we save.
    ///
    /// @note To prevent exceeding this limit, we discard older sets as needed.
    ///
    /// @note This prevents a memory leak due to some unforeseen condition
    /// where parameter sets are received and saved but never removed. I
    /// believe we should never accumulate more than two--one sequence
    /// parameter set and one picture parameter set--but due to packet loss of
    /// frames, I suppose we could temporarilly have a few more.
    static const size_t MAXIMUM_IN_BAND_PARAMETER_SETS = 10;
};
