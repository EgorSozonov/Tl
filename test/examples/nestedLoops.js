function nestedLoops() {
    lo1:
    {
    let x = 0;
    while (x < 2) {
        {
        let y = 0;
        while (y < 2) {
            {
            let z = 0;
            let a = x + y;
            while (z < 2) {
                x = x + 1;
                y = y + 1;
                z = z + 1;
                print((((`z = ` + (z).toString()) + (`; y = ` + (y).toString())) + (`; x = ` + (x).toString())));
                if (x > 0) {
                    break lo1;
                }
            }}
        }}
    }}
}

function main() {
    nestedLoops();
}
