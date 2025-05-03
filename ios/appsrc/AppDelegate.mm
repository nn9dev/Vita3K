//
//  AppDelegate.m
//  sample
//
//  Created by good afternoon on 12/11/24.
//

#import "AppDelegate.h"
#include "app/functions.h"

@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    int argc = 1;
    char *argv[5]{};
    NSURL *nsUrl = [launchOptions objectForKey:UIApplicationLaunchOptionsURLKey];

    if (nsUrl != nullptr && nsUrl.isFileURL) {
        NSString *nsString = nsUrl.path;
        const char *string = nsString.UTF8String;
        argv[argc++] = (char*)string;
    }

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioSessionInterruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleMediaServicesWereReset:) name:AVAudioSessionMediaServicesWereResetNotification object:nil];

    return [self launchVita3K:argc argv:argv];
}

- (BOOL)launchVita3K:(int)argc argv:(char**)argv {
    NSString *documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString *bundlePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingString:@"/assets/"];
    //NativeInit(argc, (const char**)argv, documentsPath.UTF8String, bundlePath.UTF8String, NULL);

    return alias_main(argc,argv);
    
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    // Make Metal view controller due to Vulkan backend being the only one available
    PPSSPPViewControllerMetal *vc = [[PPSSPPViewControllerMetal alloc] init];

    self.viewController = vc;
    self.window.rootViewController = vc;


    [self.window makeKeyAndVisible];

    return YES;
}


#pragma mark - UISceneSession lifecycle


- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options {
    // Called when a new scene session is being created.
    // Use this method to select a configuration to create the new scene with.
    return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}


- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
    // Called when the user discards a scene session.
    // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
    // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
}


@end
