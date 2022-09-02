const std = @import("std");
const Ar = @import("utils/Arena.zig");

pub fn main() anyerror!void {
    std.log.info("All your codebase are belong to us.", .{});
    var arena: *Ar.Arena = try Ar.mkArena();
    arena.delete();
}



test "basic test" {
    try std.testing.expectEqual(10, 3 + 7);
}
