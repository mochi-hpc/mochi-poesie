// Example class
function MyClass(x, y, z, t) {
    this.x = x;
    this.y = y;
    this.z = z;
    this.t = t;
    this.transform = function(v) {
        this.x = this.x + v;
        this.y = "hello " + this.y;
        this.z.sort();
        this.t = !this.t;
        return this;
    };
}

// Example object
var my_obj = new MyClass(10, "world", [3, 1, 2], true);
