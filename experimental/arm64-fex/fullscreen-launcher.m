// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <libproc.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

extern char **environ;

static NSArray<NSNumber *> *gameProcesses(NSString *root)
{
    int count = proc_listallpids(NULL, 0);
    if (count <= 0) return nil;
    int capacity = count + 128;
    pid_t *pids = calloc((size_t)capacity, sizeof(*pids));
    if (!pids) return nil;
    count = proc_listallpids(pids, capacity * (int)sizeof(*pids));
    if (count <= 0) { free(pids); return nil; }
    NSMutableArray *games = [NSMutableArray array];
    NSString *prefix = [root stringByAppendingString:@"/"];
    for (int i = 0; i < count && i < capacity; ++i)
    {
        char name[256] = {0}, path[PROC_PIDPATHINFO_MAXSIZE] = {0};
        if (proc_name(pids[i], name, sizeof(name)) <= 0 || strcmp(name, "Settlers7R.exe")) continue;
        if (proc_pidpath(pids[i], path, sizeof(path)) <= 0) continue;
        if ([[NSString stringWithUTF8String:path] hasPrefix:prefix]) [games addObject:@(pids[i])];
    }
    free(pids);
    return games;
}

static AXUIElementRef gameWindow(pid_t pid)
{
    AXUIElementRef app = AXUIElementCreateApplication(pid), result = NULL;
    CFTypeRef windows = NULL;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, &windows) == kAXErrorSuccess &&
        windows && CFGetTypeID(windows) == CFArrayGetTypeID())
    {
        for (id item in (__bridge NSArray *)windows)
        {
            AXUIElementRef window = (__bridge AXUIElementRef)item;
            CFTypeRef title = NULL;
            AXUIElementCopyAttributeValue(window, kAXTitleAttribute, &title);
            BOOL match = title && [(__bridge id)title isEqual:@"Settlers 7 Window"];
            if (title) CFRelease(title);
            if (match) { result = (AXUIElementRef)CFRetain(window); break; }
        }
    }
    if (windows) CFRelease(windows);
    CFRelease(app);
    return result;
}

static BOOL isFullscreen(AXUIElementRef window)
{
    CFTypeRef value = NULL;
    AXUIElementCopyAttributeValue(window, CFSTR("AXFullScreen"), &value);
    BOOL result = value && CFEqual(value, kCFBooleanTrue);
    if (value) CFRelease(value);
    return result;
}

static void activateGame(pid_t pid)
{
    BOOL accepted = [[NSRunningApplication runningApplicationWithProcessIdentifier:pid]
        activateWithOptions:NSApplicationActivateAllWindows];
    printf("[fullscreen-launch] activation-request pid=%d accepted=%d\n", pid, accepted);
    fflush(stdout);
}

static int readOption(NSString *text, NSString *key)
{
    int value = -1, matches = 0;
    for (NSString *line in [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet])
    {
        NSArray *parts = [line componentsSeparatedByString:@"="];
        if (parts.count != 2) continue;
        NSString *name = [parts[0] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        if (![name isEqualToString:key]) continue;
        NSString *number = [parts[1] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        NSScanner *scanner = [NSScanner scannerWithString:number];
        if (![scanner scanInt:&value] || !scanner.isAtEnd) return -1;
        ++matches;
    }
    return matches == 1 ? value : -1;
}

static int resizeClient(NSString *runner, NSString *helper, int width, int height)
{
    NSString *windowsHelper = [@"Z:" stringByAppendingString:[helper stringByReplacingOccurrencesOfString:@"/" withString:@"\\"]];
    NSString *w = [NSString stringWithFormat:@"%d", width], *h = [NSString stringWithFormat:@"%d", height];
    char *command[] = {"/usr/bin/python3", (char *)runner.fileSystemRepresentation,
        (char *)windowsHelper.UTF8String, (char *)w.UTF8String, (char *)h.UTF8String, NULL};
    pid_t child = 0;
    int status = 0, result = posix_spawn(&child, command[0], NULL, NULL, command, environ);
    if (result) return result;
    for (int i = 0; i < 150; ++i)
    {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) return WIFEXITED(status) ? WEXITSTATUS(status) : 20;
        if (waited < 0 && errno != EINTR) return 21;
        usleep(100000);
    }
    // Only terminate the helper process we started, never Wine's server or the game.
    kill(child, SIGTERM);
    for (int i = 0; i < 20; ++i)
    {
        if (waitpid(child, &status, WNOHANG) == child) return 22;
        usleep(100000);
    }
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return 22;
}

int main(int argc, char **argv)
{
    @autoreleasepool
    {
        if (argc != 3) { fputs("Usage: fullscreen-launcher RUNTIME_ROOT OPTIONS_INI\n", stderr); return 2; }
        NSString *root = [[NSString stringWithUTF8String:argv[1]] stringByResolvingSymlinksInPath];
        NSString *options = [[NSString stringWithUTF8String:argv[2]] stringByResolvingSymlinksInPath];
        NSString *runner = [root stringByAppendingPathComponent:@"run.py"];
        NSString *helper = [root stringByAppendingPathComponent:@"bin/resize-client.exe"];
        if (![root isAbsolutePath] || ![options hasPrefix:[root stringByAppendingString:@"/prefix/"]] ||
            ![NSFileManager.defaultManager isReadableFileAtPath:runner] ||
            ![NSFileManager.defaultManager isReadableFileAtPath:helper]) return 3;
        NSString *text = [NSString stringWithContentsOfFile:options encoding:NSUTF8StringEncoding error:NULL];
        if (!text) { fputs("Cannot read game options.\n", stderr); return 4; }
        int fullscreen = readOption(text, @"FullScreen"), width = readOption(text, @"ScreenWidth"), height = readOption(text, @"ScreenHeight");
        if (fullscreen != 0 || width < 320 || width > 8192 || height < 240 || height > 8192)
        { fputs("Native fullscreen requires FullScreen=0 and valid dimensions in Options.ini.\n", stderr); return 5; }
        CGDirectDisplayID display = CGMainDisplayID();
        if (!CGDisplayIsBuiltin(display) || CGDisplayIsInMirrorSet(display))
        { fputs("The built-in display must be the unmirrored primary display.\n", stderr); return 6; }
        BOOL accessibility = AXIsProcessTrusted();
        if (!accessibility)
        {
            NSDictionary *request = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
            accessibility = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)request);
        }

        NSString *lockPath = [root stringByAppendingPathComponent:@"logs/fullscreen-launch.lock"];
        int lock = open(lockPath.fileSystemRepresentation, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (lock < 0) return 8;
        if (flock(lock, LOCK_EX | LOCK_NB))
        {
            NSArray *games = gameProcesses(root);
            for (NSNumber *number in games)
            {
                AXUIElementRef window = gameWindow(number.intValue);
                if (window) { activateGame(number.intValue); CFRelease(window); break; }
            }
            return 0;
        }
        NSArray *games = gameProcesses(root);
        if (!games) { fputs("Cannot inspect game processes.\n", stderr); return 9; }
        pid_t launcher = 0;
        if (!games.count)
        {
            char *command[] = {"/usr/bin/python3", (char *)runner.fileSystemRepresentation, "--debug=-all,+debugstr",
                "C:\\Program Files (x86)\\Steam\\steam.exe", "-applaunch", "965320", NULL};
            int result = posix_spawn(&launcher, command[0], NULL, NULL, command, environ);
            if (result) { fprintf(stderr, "Steam launch failed: %s\n", strerror(result)); return 10; }
        }
        if (!accessibility)
        {
            if (games.count) activateGame([games.lastObject intValue]);
            fputs("[fullscreen-launch] Started normally without automatic fullscreen. Enable Accessibility for the Settlers launcher in System Settings, then reopen it.\n", stderr);
            return 0;
        }
        AXUIElementRef window = NULL;
        pid_t pid = 0;
        for (int i = 0; i < 360 && !window; ++i)
        {
            if (launcher) { int status; if (waitpid(launcher, &status, WNOHANG) == launcher) launcher = 0; }
            games = gameProcesses(root);
            for (NSNumber *number in games)
            {
                window = gameWindow(number.intValue);
                if (window) { pid = number.intValue; break; }
            }
            if (!window) usleep(500000);
        }
        if (!window) { fputs("No game window appeared within three minutes. Steam and Ubisoft remain open; if the game appears later, reopen this launcher.\n", stderr); return 11; }
        activateGame(pid);
        if (!isFullscreen(window))
        {
            CGRect screen = CGDisplayBounds(display);
            CGPoint point = CGPointMake(screen.origin.x, screen.origin.y + 60);
            AXValueRef position = AXValueCreate(kAXValueCGPointType, &point);
            AXError moved = AXUIElementSetAttributeValue(window, kAXPositionAttribute, position);
            CFRelease(position);
            AXError entered = moved == kAXErrorSuccess ?
                AXUIElementSetAttributeValue(window, CFSTR("AXFullScreen"), kCFBooleanTrue) : moved;
            if (entered != kAXErrorSuccess)
            { fprintf(stderr, "Native fullscreen request failed: %d\n", entered); CFRelease(window); return 12; }
            usleep(2000000);
        }
        if (!isFullscreen(window))
        { fputs("Native fullscreen was not confirmed.\n", stderr); CFRelease(window); return 13; }
        int resized = resizeClient(runner, helper, width, height);
        if (resized)
        { fprintf(stderr, "Client sizing failed: %d\n", resized); CFRelease(window); return 14; }
        CFTypeRef size = NULL;
        CGSize dimensions = CGSizeZero;
        AXError read = AXUIElementCopyAttributeValue(window, kAXSizeAttribute, &size);
        BOOL verified = read == kAXErrorSuccess && size && CFGetTypeID(size) == AXValueGetTypeID() &&
            AXValueGetValue(size, kAXValueCGSizeType, &dimensions) &&
            fabs(dimensions.width - width) <= 1 && fabs(dimensions.height - height) <= 1;
        if (size) CFRelease(size);
        CFRelease(window);
        if (!verified) { fputs("Native window size was not confirmed.\n", stderr); return 15; }
        printf("[fullscreen-launch] ready pid=%d client=%dx%d native-fullscreen=1\n", pid, width, height);
        return 0;
    }
}
