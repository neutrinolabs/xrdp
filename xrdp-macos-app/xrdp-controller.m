//
//  xrdp-controller.m
//  xrdp Remote Desktop
//
//  Copyright (C) 2026 Neutrinos Software Corporation
//  Some portions Classify®
//

#import <Cocoa/Cocoa.h>

@interface XRDPController : NSObject <NSApplicationDelegate>
@property (strong, nonatomic) NSStatusItem *statusItem;
@property (strong, nonatomic) NSMenu *statusMenu;
@property (strong, nonatomic) NSTask *xrdpTask;
@property (strong, nonatomic) NSTask *sesmanTask;
@property (strong, nonatomic) NSTask *chansrvTask;
@property (strong, nonatomic) NSMenuItem *statusMenuItem;
@property (strong, nonatomic) NSMenuItem *startMenuItem;
@property (strong, nonatomic) NSMenuItem *stopMenuItem;
@end

@implementation XRDPController

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Create status bar item
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    self.statusItem.button.title = @"⚡️";
    self.statusItem.button.toolTip = @"xrdp Remote Desktop";

    // Create menu
    self.statusMenu = [[NSMenu alloc] init];

    self.statusMenuItem = [[NSMenuItem alloc] initWithTitle:@"xrdp Server: Starting..." action:nil keyEquivalent:@""];
    [self.statusMenuItem setEnabled:NO];
    [self.statusMenu addItem:self.statusMenuItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    self.startMenuItem = [[NSMenuItem alloc] initWithTitle:@"Start Server" action:@selector(startServer:) keyEquivalent:@""];
    self.startMenuItem.target = self;
    [self.statusMenu addItem:self.startMenuItem];

    self.stopMenuItem = [[NSMenuItem alloc] initWithTitle:@"Stop Server" action:@selector(stopServer:) keyEquivalent:@""];
    self.stopMenuItem.target = self;
    [self.statusMenu addItem:self.stopMenuItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(quit:) keyEquivalent:@"q"];
    quitItem.target = self;
    [self.statusMenu addItem:quitItem];

    self.statusItem.menu = self.statusMenu;

    // Initial state
    [self updateMenuState:NO];

    // Kill any existing xrdp processes
    [self killExistingProcesses];

    // Auto-start server
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self startServer:nil];
    });

    // Check for Screen Recording permission
    [self checkScreenRecordingPermission];
}

- (void)checkScreenRecordingPermission {
    if (@available(macOS 10.15, *)) {
        CGDisplayStreamRef stream = CGDisplayStreamCreate(CGMainDisplayID(), 1, 1,
                                                         kCVPixelFormatType_32BGRA, nil,
                                                         ^(CGDisplayStreamFrameStatus status,
                                                          uint64_t displayTime,
                                                          IOSurfaceRef frameSurface,
                                                          CGDisplayStreamUpdateRef updateRef) {});
        if (stream) {
            CFRelease(stream);
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"Screen Recording Permission Required";
                alert.informativeText = @"xrdp needs Screen Recording permission.\n\nGrant in System Settings → Privacy & Security → Screen Recording";
                [alert addButtonWithTitle:@"Open Settings"];
                [alert addButtonWithTitle:@"Later"];

                NSModalResponse response = [alert runModal];
                if (response == NSAlertFirstButtonReturn) {
                    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture"]];
                }
            });
        }
    }
}

- (NSString *)helperPath:(NSString *)name {
    NSString *helpersDir = [[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"Helpers"];
    return [helpersDir stringByAppendingPathComponent:name];
}

- (NSString *)configPath {
    return [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"etc/xrdp/xrdp.ini"];
}

- (NSString *)sesmanConfigPath {
    return [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"etc/xrdp/sesman.ini"];
}

- (void)killExistingProcesses {
    NSLog(@"Checking for existing xrdp server processes...");

    // Kill xrdp server processes in Helpers directory (not the app itself)
    NSString *helpersDir = [[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"Helpers"];

    // Kill xrdp server
    NSTask *killXrdp = [[NSTask alloc] init];
    killXrdp.launchPath = @"/usr/bin/pkill";
    killXrdp.arguments = @[@"-9", @"-f", [helpersDir stringByAppendingPathComponent:@"xrdp"]];
    @try {
        [killXrdp launch];
        [killXrdp waitUntilExit];
    } @catch (NSException *exception) {
        // Ignore - process may not exist
    }

    // Kill xrdp-sesman
    NSTask *killSesman = [[NSTask alloc] init];
    killSesman.launchPath = @"/usr/bin/pkill";
    killSesman.arguments = @[@"-9", @"-f", [helpersDir stringByAppendingPathComponent:@"xrdp-sesman"]];
    @try {
        [killSesman launch];
        [killSesman waitUntilExit];
    } @catch (NSException *exception) {
        // Ignore - process may not exist
    }

    // Kill xrdp-chansrv
    NSTask *killChansrv = [[NSTask alloc] init];
    killChansrv.launchPath = @"/usr/bin/pkill";
    killChansrv.arguments = @[@"-9", @"-f", [helpersDir stringByAppendingPathComponent:@"xrdp-chansrv"]];
    @try {
        [killChansrv launch];
        [killChansrv waitUntilExit];
    } @catch (NSException *exception) {
        // Ignore - process may not exist
    }

    // Wait a moment for processes to terminate
    [NSThread sleepForTimeInterval:0.3];
    NSLog(@"Cleanup complete");
}

- (void)startServer:(id)sender {
    if (self.xrdpTask && self.xrdpTask.isRunning) {
        return;
    }

    // Create runtime directory inside app bundle for sesman socket
    NSString *runPath = [[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"run"];
    [[NSFileManager defaultManager] createDirectoryAtPath:runPath
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];

    // Start xrdp-sesman first
    self.sesmanTask = [[NSTask alloc] init];
    self.sesmanTask.launchPath = [self helperPath:@"xrdp-sesman"];
    self.sesmanTask.arguments = @[@"--nodaemon", @"-c", [self sesmanConfigPath]];

    // Set environment
    NSMutableDictionary *env = [[[NSProcessInfo processInfo] environment] mutableCopy];
    env[@"DYLD_LIBRARY_PATH"] = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"lib"];
    self.sesmanTask.environment = env;

    @try {
        [self.sesmanTask launch];
        NSLog(@"Started xrdp-sesman (PID: %d)", self.sesmanTask.processIdentifier);
    } @catch (NSException *exception) {
        NSLog(@"Failed to start xrdp-sesman: %@", exception);
        [self showAlert:@"Failed to start session manager" message:exception.reason];
        return;
    }

    // Wait a moment for sesman to initialize
    [NSThread sleepForTimeInterval:0.5];

    // Start xrdp server
    self.xrdpTask = [[NSTask alloc] init];
    self.xrdpTask.launchPath = [self helperPath:@"xrdp"];
    self.xrdpTask.arguments = @[@"--nodaemon", @"--port", @"3389", @"-c", [self configPath]];
    self.xrdpTask.environment = env;

    @try {
        [self.xrdpTask launch];
        NSLog(@"Started xrdp (PID: %d)", self.xrdpTask.processIdentifier);
        [self updateMenuState:YES];
        NSLog(@"Note: xrdp-chansrv will be started automatically by sesman when a user connects");
    } @catch (NSException *exception) {
        NSLog(@"Failed to start xrdp: %@", exception);
        [self showAlert:@"Failed to start xrdp server" message:exception.reason];

        // Stop sesman too
        if (self.sesmanTask && self.sesmanTask.isRunning) {
            [self.sesmanTask terminate];
        }
    }
}

- (void)stopServer:(id)sender {
    NSLog(@"Stopping xrdp server...");

    // Terminate tasks gracefully first
    if (self.chansrvTask && self.chansrvTask.isRunning) {
        [self.chansrvTask terminate];
        self.chansrvTask = nil;
    }

    if (self.xrdpTask && self.xrdpTask.isRunning) {
        [self.xrdpTask terminate];
        self.xrdpTask = nil;
    }

    if (self.sesmanTask && self.sesmanTask.isRunning) {
        [self.sesmanTask terminate];
        self.sesmanTask = nil;
    }

    // Ensure all processes are killed
    [self killExistingProcesses];

    [self updateMenuState:NO];
}

- (void)updateMenuState:(BOOL)running {
    if (running) {
        self.statusMenuItem.title = @"xrdp Server: Running";
        self.statusItem.button.title = @"⚡️";
        [self.startMenuItem setEnabled:NO];
        [self.stopMenuItem setEnabled:YES];
    } else {
        self.statusMenuItem.title = @"xrdp Server: Stopped";
        self.statusItem.button.title = @"⚡️";
        [self.startMenuItem setEnabled:YES];
        [self.stopMenuItem setEnabled:NO];
    }
}

- (void)showAlert:(NSString *)title message:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = title;
    alert.informativeText = message;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)quit:(id)sender {
    [self stopServer:nil];
    [NSApp terminate:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    [self stopServer:nil];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        XRDPController *controller = [[XRDPController alloc] init];
        app.delegate = controller;
        [app run];
    }
    return 0;
}
