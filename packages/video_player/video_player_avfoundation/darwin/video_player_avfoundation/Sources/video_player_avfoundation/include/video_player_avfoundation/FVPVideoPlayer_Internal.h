// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AVFoundation/AVFoundation.h>
#import "FVPAVFactory.h"
#import "FVPVideoEventListener.h"
#import "FVPVideoPlayer.h"
#import "FVPViewProvider.h"

#if TARGET_OS_IOS
#import <AVKit/AVKit.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@interface FVPVideoPlayer ()
@property(nonatomic, readonly) AVPlayerItemVideoOutput *videoOutput;
@property(nonatomic, readonly, nullable) NSObject<FVPViewProvider> *viewProvider;
@property(nonatomic) CGAffineTransform preferredTransform;
@property(nonatomic, readonly) NSNumber *targetPlaybackSpeed;
@property(nonatomic, readonly) BOOL isPlaying;
@property(nonatomic, readonly) BOOL isInitialized;

#if TARGET_OS_IOS
@property(nonatomic, strong, nullable) AVPictureInPictureController *pipController;
@property(nonatomic, strong, nullable) AVPlayerLayer *pipPlayerLayer;
#endif

- (void)updatePlayingState;
@end

NS_ASSUME_NONNULL_END
