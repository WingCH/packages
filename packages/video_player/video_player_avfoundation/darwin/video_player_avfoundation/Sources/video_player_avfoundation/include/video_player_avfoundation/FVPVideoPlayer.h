// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AVFoundation/AVFoundation.h>

#if TARGET_OS_IOS
#import <AVKit/AVKit.h>
#endif

#import "./messages.g.h"
#import "FVPAVFactory.h"
#import "FVPVideoEventListener.h"
#import "FVPViewProvider.h"

#if TARGET_OS_OSX
#import <FlutterMacOS/FlutterMacOS.h>
#else
#import <Flutter/Flutter.h>
#endif

NS_ASSUME_NONNULL_BEGIN

#if TARGET_OS_IOS
@interface FVPVideoPlayer
    : NSObject <FVPVideoPlayerInstanceApi, AVPictureInPictureControllerDelegate>
#else
@interface FVPVideoPlayer : NSObject <FVPVideoPlayerInstanceApi>
#endif
/// The AVPlayer instance used for video playback.
@property(nonatomic, readonly) AVPlayer *player;
/// Indicates whether the video player has been disposed.
@property(nonatomic, readonly) BOOL disposed;
/// Indicates whether the video player is set to loop.
@property(nonatomic) BOOL isLooping;
/// The current playback position of the video, in milliseconds.
@property(nonatomic, readonly) int64_t position;
/// The event listener to report video events to.
@property(nonatomic, nullable) NSObject<FVPVideoEventListener> *eventListener;
/// A block that will be called when dispose is called.
@property(nonatomic, nullable, copy) void (^onDisposed)(void);

/// Initializes a new instance of FVPVideoPlayer with the given AVPlayerItem, AV factory, and view
/// provider.
- (instancetype)initWithPlayerItem:(AVPlayerItem *)item
                         avFactory:(id<FVPAVFactory>)avFactory
                      viewProvider:(NSObject<FVPViewProvider> *)viewProvider;

#if TARGET_OS_IOS
/// Sets up Picture in Picture with the given player layer.
/// Call this after the player layer is available.
- (void)setupPictureInPictureWithPlayerLayer:(AVPlayerLayer *)playerLayer;
#endif

/// Called when the video player is initialized and ready to play.
/// Subclasses can override this method to perform additional setup.
- (void)reportInitialized;

@end

NS_ASSUME_NONNULL_END
