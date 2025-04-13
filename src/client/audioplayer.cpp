#include "include/audioplayer.h"
#import <AVFoundation/AVFoundation.h>

// Wrap Objective-C in extern "C" to prevent C++ name mangling
extern "C" {
    typedef struct {
        AVAudioEngine* engine;
        AVAudioPlayerNode* player;
        AVAudioFile* file;
        AVAudioPCMBuffer* buffer;
        uint64_t startTime;
    } AudioPlayerImpl;
}

AudioPlayer::AudioPlayer() {
    impl = new AudioPlayerImpl{
        .engine = [[AVAudioEngine alloc] init],
        .player = [[AVAudioPlayerNode alloc] init],
        .file = nil,
        .buffer = nil,
        .startTime = 0
    };
    [impl->engine attachNode:impl->player];
    [impl->engine connect:impl->player to:impl->engine.mainOutputNode format:nil];
}

AudioPlayer::~AudioPlayer() {
    [impl->player stop];
    [impl->engine stop];
    delete impl;
}

bool AudioPlayer::load(const std::string& filePath) {
    NSString* nsPath = [NSString stringWithUTF8String:filePath.c_str()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];
    NSError* error = nil;
    
    impl->file = [[AVAudioFile alloc] initForReading:url error:&error];
    if (error) {
        NSLog(@"Failed to load file: %@", error);
        return false;
    }

    impl->buffer = [[AVAudioPCMBuffer alloc] 
        initWithPCMFormat:impl->file.processingFormat
            frameCapacity:(AVAudioFrameCount)impl->file.length];
    [impl->file readIntoBuffer:impl->buffer error:&error];
    
    return error == nil;
}

void AudioPlayer::play() {
    if (!impl->file) return;
    
    impl->startTime = mach_absolute_time();
    [impl->engine startAndReturnError:nil];
    [impl->player play];
    [impl->player scheduleBuffer:impl->buffer atTime:nil options:AVAudioPlayerNodeBufferLoops completionHandler:nil];
}
void AudioPlayer::pause() {
    [impl->player pause];
}

void AudioPlayer::stop() {
    [impl->player stop];
    impl->startTime = 0;
}

double AudioPlayer::getPosition() const {
    if (!impl->player.playing) return 0.0;
    
    uint64_t now = mach_absolute_time();
    return static_cast<double>(now - impl->startTime) / 1e9;  // Convert nanoseconds to seconds
}

bool AudioPlayer::isPlaying() const {
    return impl->player.playing;
}