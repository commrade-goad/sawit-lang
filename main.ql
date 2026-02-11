// for the future block will be expr
// so `let a = { a + 20 };` is possible but not for now.
let a = 10;
{
    let b = 5;
    a = a + b;
}
a;

let a = 10 * (20 + 3);
