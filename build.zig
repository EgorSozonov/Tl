const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    b.setPreferredReleaseMode(.Debug);
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("tlco", "source/main.zig");    
    exe.setOutputDir("_bin");
    exe.setTarget(target);
    exe.setBuildMode(mode);
    exe.install();

    const runCmd = exe.run();
    runCmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        runCmd.addArgs(args);
    }

    const runStep = b.step("run", "Run the app");
    runStep.dependOn(&runCmd.step);

    const exeTests = b.addTest("source/main.zig");
    exeTests.setTarget(target);
    exeTests.setBuildMode(mode);

    const testStep = b.step("test", "Run unit tests");
    testStep.dependOn(&exeTests.step);
}
