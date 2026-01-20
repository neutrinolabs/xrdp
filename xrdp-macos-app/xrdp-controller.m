//
//  xrdp-controller.m
//  xrdp Remote Desktop
//
//  Copyright (C) 2026 Neutrinos Software Corporation
//  Some portions Classify®
//

#import <Cocoa/Cocoa.h>

@interface XRDPSession : NSObject
@property (strong, nonatomic) NSString *sessionId;
@property (strong, nonatomic) NSString *username;
@property (strong, nonatomic) NSString *ipAddress;
@property (strong, nonatomic) NSString *protocol;
@property (strong, nonatomic) NSString *encryption;
@property (assign, nonatomic) NSInteger pid;
@property (strong, nonatomic) NSDate *connectedAt;
@end

@implementation XRDPSession
@end

@interface XRDPController : NSObject <NSApplicationDelegate>
@property (strong, nonatomic) NSStatusItem *statusItem;
@property (strong, nonatomic) NSMenu *statusMenu;
@property (strong, nonatomic) NSTask *xrdpTask;
@property (strong, nonatomic) NSTask *sesmanTask;
@property (strong, nonatomic) NSTask *chansrvTask;
@property (strong, nonatomic) NSMenuItem *statusMenuItem;
@property (strong, nonatomic) NSMenuItem *connectionsMenuItem;
@property (strong, nonatomic) NSMenuItem *startMenuItem;
@property (strong, nonatomic) NSMenuItem *stopMenuItem;
@property (strong, nonatomic) NSTimer *connectionMonitor;
@property (strong, nonatomic) NSMutableArray<XRDPSession *> *activeSessions;
@property (assign, nonatomic) NSInteger connectionCount;
@end

@implementation XRDPController

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Initialize session tracking
    self.activeSessions = [NSMutableArray array];
    self.connectionCount = 0;

    // Create status bar item with proper clickable icon
    // Use NSSquareStatusItemLength to ensure full clickable area
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];

    // Use SF Symbol for server icon
    if (@available(macOS 11.0, *)) {
        // Try multiple symbols in order of preference
        NSImage *icon = [NSImage imageWithSystemSymbolName:@"atom" accessibilityDescription:@"xrdp"];
        if (!icon) {
            icon = [NSImage imageWithSystemSymbolName:@"server.rack" accessibilityDescription:@"xrdp"];
        }
        if (!icon) {
            icon = [NSImage imageWithSystemSymbolName:@"network" accessibilityDescription:@"xrdp"];
        }
        if (!icon) {
            icon = [NSImage imageWithSystemSymbolName:@"circle.hexagongrid" accessibilityDescription:@"xrdp"];
        }
        if (icon) {
            [icon setTemplate:YES];
            self.statusItem.button.image = icon;
            self.statusItem.button.title = nil; // Explicitly clear title when using image
            NSLog(@"Using SF Symbol icon for menu bar");
        } else {
            self.statusItem.button.title = @"⚛";
            self.statusItem.button.image = nil; // Clear image when using text
            NSLog(@"Fallback to text glyph for menu bar");
        }
    } else {
        self.statusItem.button.title = @"⚛";
        NSLog(@"macOS < 11.0: using text glyph");
    }

    self.statusItem.button.toolTip = @"xrdp Remote Desktop";

    // Ensure button is enabled and visible
    [self.statusItem.button setEnabled:YES];
    self.statusItem.visible = YES;

    // Create menu
    self.statusMenu = [[NSMenu alloc] init];

    self.statusMenuItem = [[NSMenuItem alloc] initWithTitle:@"xrdp Server: Starting..." action:nil keyEquivalent:@""];
    [self.statusMenuItem setEnabled:NO];
    [self.statusMenu addItem:self.statusMenuItem];

    // Connections submenu
    self.connectionsMenuItem = [[NSMenuItem alloc] initWithTitle:@"Active Connections: 0" action:nil keyEquivalent:@""];
    [self.connectionsMenuItem setEnabled:NO];
    [self.statusMenu addItem:self.connectionsMenuItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    self.startMenuItem = [[NSMenuItem alloc] initWithTitle:@"Start Server" action:@selector(startServer:) keyEquivalent:@""];
    self.startMenuItem.target = self;
    [self.statusMenu addItem:self.startMenuItem];

    self.stopMenuItem = [[NSMenuItem alloc] initWithTitle:@"Stop Server" action:@selector(stopServer:) keyEquivalent:@""];
    self.stopMenuItem.target = self;
    [self.statusMenu addItem:self.stopMenuItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *aboutItem = [[NSMenuItem alloc] initWithTitle:@"About xrdp..." action:@selector(showAbout:) keyEquivalent:@""];
    aboutItem.target = self;
    [self.statusMenu addItem:aboutItem];

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

        // Start connection monitoring
        [self startConnectionMonitoring];
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

    // Stop connection monitoring
    [self stopConnectionMonitoring];

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
        [self.startMenuItem setEnabled:NO];
        [self.stopMenuItem setEnabled:YES];
    } else {
        self.statusMenuItem.title = @"xrdp Server: Stopped";
        [self.startMenuItem setEnabled:YES];
        [self.stopMenuItem setEnabled:NO];
    }

    // Update icon to default state (unless connections update it)
    if (@available(macOS 11.0, *)) {
        if (self.statusItem.button.image) {
            // Keep using image
        }
    } else {
        self.statusItem.button.title = @"⚛";
    }
}

- (void)showAlert:(NSString *)title message:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = title;
    alert.informativeText = message;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)startConnectionMonitoring {
    // Start monitoring every 5 seconds
    self.connectionMonitor = [NSTimer scheduledTimerWithTimeInterval:5.0
                                                             target:self
                                                           selector:@selector(updateConnections)
                                                           userInfo:nil
                                                            repeats:YES];
    // Trigger immediate update
    [self updateConnections];
}

- (void)stopConnectionMonitoring {
    if (self.connectionMonitor) {
        [self.connectionMonitor invalidate];
        self.connectionMonitor = nil;
    }
    [self.activeSessions removeAllObjects];
    self.connectionCount = 0;
    [self updateConnectionsDisplay];
}

- (void)updateConnections {
    // Get list of xrdp connection processes
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/bin/ps";
    task.arguments = @[@"aux"];

    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;

    @try {
        [task launch];
        [task waitUntilExit];

        NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
        NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

        [self parseConnectionInfo:output];
    } @catch (NSException *exception) {
        NSLog(@"Failed to get connection info: %@", exception);
    }
}

- (void)parseConnectionInfo:(NSString *)psOutput {
    NSMutableArray<XRDPSession *> *newSessions = [NSMutableArray array];

    NSArray *lines = [psOutput componentsSeparatedByString:@"\n"];
    for (NSString *line in lines) {
        if ([line containsString:@"xrdp-chansrv"] && ![line containsString:@"grep"]) {
            XRDPSession *session = [self parseSessionFromProcessLine:line];
            if (session) {
                [newSessions addObject:session];
            }
        }
    }

    self.activeSessions = newSessions;
    self.connectionCount = newSessions.count;
    [self updateConnectionsDisplay];
}

- (XRDPSession *)parseSessionFromProcessLine:(NSString *)line {
    // Parse process line: user PID ... /path/to/xrdp-chansrv
    NSArray *components = [line componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSMutableArray *filtered = [NSMutableArray array];
    for (NSString *comp in components) {
        if (comp.length > 0) [filtered addObject:comp];
    }

    if (filtered.count < 2) return nil;

    XRDPSession *session = [[XRDPSession alloc] init];
    session.username = filtered[0];
    session.pid = [filtered[1] integerValue];
    session.protocol = @"RDP";
    session.encryption = @"TLS 1.3 ChaCha20-Poly1305";
    session.connectedAt = [NSDate date];

    // Try to get IP address from lsof
    [self getIPAddressForPID:session.pid session:session];

    return session;
}

- (void)getIPAddressForPID:(NSInteger)pid session:(XRDPSession *)session {
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/sbin/lsof";
    task.arguments = @[@"-p", [NSString stringWithFormat:@"%ld", (long)pid], @"-n", @"-P", @"-iTCP"];

    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    task.standardError = [NSPipe pipe];

    @try {
        [task launch];
        [task waitUntilExit];

        NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
        NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

        NSArray *lines = [output componentsSeparatedByString:@"\n"];
        for (NSString *line in lines) {
            if ([line containsString:@"->"]) {
                NSRange range = [line rangeOfString:@"->"];
                if (range.location != NSNotFound) {
                    NSString *afterArrow = [line substringFromIndex:range.location + 2];
                    NSArray *parts = [afterArrow componentsSeparatedByString:@":"];
                    if (parts.count > 0) {
                        session.ipAddress = parts[0];
                        break;
                    }
                }
            }
        }
    } @catch (NSException *exception) {
        // Ignore
    }

    if (!session.ipAddress) {
        session.ipAddress = @"127.0.0.1";
    }
}

- (void)updateConnectionsDisplay {
    // Update menu icon with connection count
    if (@available(macOS 11.0, *)) {
        if (self.statusItem.button.image) {
            // Use image - explicitly ensure no title is set
            self.statusItem.button.title = nil;

            // Show connection count in tooltip instead
            if (self.connectionCount > 0) {
                self.statusItem.button.toolTip = [NSString stringWithFormat:@"xrdp Remote Desktop - %ld active connection%@",
                                                  (long)self.connectionCount,
                                                  self.connectionCount == 1 ? @"" : @"s"];
            } else {
                self.statusItem.button.toolTip = @"xrdp Remote Desktop";
            }
        } else {
            // Fallback to text only
            if (self.connectionCount > 0) {
                self.statusItem.button.title = [NSString stringWithFormat:@"⚛ %ld", (long)self.connectionCount];
            } else {
                self.statusItem.button.title = @"⚛";
            }
        }
    } else {
        // macOS < 11.0: use text
        if (self.connectionCount > 0) {
            self.statusItem.button.title = [NSString stringWithFormat:@"⚛ %ld", (long)self.connectionCount];
        } else {
            self.statusItem.button.title = @"⚛";
        }
    }

    // Update connections menu item
    self.connectionsMenuItem.title = [NSString stringWithFormat:@"Active Connections: %ld", (long)self.connectionCount];

    // Rebuild submenu with session details
    NSMenu *connectionsSubmenu = [[NSMenu alloc] init];

    if (self.connectionCount == 0) {
        NSMenuItem *noConnections = [[NSMenuItem alloc] initWithTitle:@"No active connections" action:nil keyEquivalent:@""];
        [noConnections setEnabled:NO];
        [connectionsSubmenu addItem:noConnections];
    } else {
        for (XRDPSession *session in self.activeSessions) {
            NSString *title = [NSString stringWithFormat:@"%@ from %@", session.username, session.ipAddress ?: @"unknown"];
            NSMenuItem *sessionItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];

            // Create submenu with details
            NSMenu *detailsMenu = [[NSMenu alloc] init];

            NSMenuItem *protocolItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Protocol: %@", session.protocol] action:nil keyEquivalent:@""];
            [protocolItem setEnabled:NO];
            [detailsMenu addItem:protocolItem];

            NSMenuItem *encryptionItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Encryption: %@", session.encryption] action:nil keyEquivalent:@""];
            [encryptionItem setEnabled:NO];
            [detailsMenu addItem:encryptionItem];

            NSMenuItem *pidItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"PID: %ld", (long)session.pid] action:nil keyEquivalent:@""];
            [pidItem setEnabled:NO];
            [detailsMenu addItem:pidItem];

            [detailsMenu addItem:[NSMenuItem separatorItem]];

            NSMenuItem *disconnectItem = [[NSMenuItem alloc] initWithTitle:@"Disconnect Session" action:@selector(disconnectSession:) keyEquivalent:@""];
            disconnectItem.target = self;
            disconnectItem.representedObject = session;
            [detailsMenu addItem:disconnectItem];

            sessionItem.submenu = detailsMenu;
            [connectionsSubmenu addItem:sessionItem];
        }
    }

    self.connectionsMenuItem.submenu = connectionsSubmenu;
    [self.connectionsMenuItem setEnabled:YES];
}

- (void)disconnectSession:(NSMenuItem *)sender {
    XRDPSession *session = sender.representedObject;
    if (!session) return;

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Disconnect User?";
    alert.informativeText = [NSString stringWithFormat:@"Are you sure you want to disconnect %@ from %@?", session.username, session.ipAddress];
    [alert addButtonWithTitle:@"Disconnect"];
    [alert addButtonWithTitle:@"Cancel"];

    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        // Kill the chansrv process
        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/bin/kill";
        task.arguments = @[@"-9", [NSString stringWithFormat:@"%ld", (long)session.pid]];

        @try {
            [task launch];
            [task waitUntilExit];
            NSLog(@"Disconnected session for %@ (PID: %ld)", session.username, (long)session.pid);

            // Force immediate update
            [self updateConnections];
        } @catch (NSException *exception) {
            NSLog(@"Failed to disconnect session: %@", exception);
        }
    }
}

- (void)showAbout:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"⚛ xrdp for macOS";

    NSString *aboutText = @"xrdp - Open Source Remote Desktop Protocol Server\n"
                          @"Version: 1.0.0\n\n"
                          @"Built with NeutrinoTLS - Pure C TLS 1.3 Implementation\n"
                          @"Using ChaCha20-Poly1305 AEAD Encryption\n\n"
                          @"ACKNOWLEDGEMENTS\n\n"
                          @"xrdp Project:\n"
                          @"  Copyright © 2004-2024 Jay Sorg and all contributors\n"
                          @"  Licensed under Apache License 2.0\n\n"
                          @"NeutrinoTLS:\n"
                          @"  Pure C implementation of TLS 1.3 (RFC 8446)\n"
                          @"  ChaCha20-Poly1305 (RFC 7539)\n"
                          @"  X25519 Key Exchange (RFC 7748)\n"
                          @"  HKDF-SHA256 (RFC 5869)\n\n"
                          @"OpenSSL:\n"
                          @"  Copyright © 1998-2024 The OpenSSL Project\n"
                          @"  Licensed under Apache License 2.0\n\n"
                          @"macOS Integration:\n"
                          @"  Copyright © 2026 Neutrinos Software Corporation\n"
                          @"  Some portions Classify®\n\n"
                          @"LICENSES\n\n"
                          @"xrdp: Apache License 2.0\n"
                          @"OpenSSL: Apache License 2.0\n"
                          @"NeutrinoTLS: Apache License 2.0\n\n"
                          @"Full license text available at:\n"
                          @"https://www.apache.org/licenses/LICENSE-2.0\n\n"
                          @"⚛ Built with Claude Code";

    alert.informativeText = aboutText;
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"View on GitHub"];

    NSModalResponse response = [alert runModal];
    if (response == NSAlertSecondButtonReturn) {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://github.com/neutrinolabs/xrdp"]];
    }
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
