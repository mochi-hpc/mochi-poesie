# Define the class
class MyClass

    def initialize(x, y, z, t)
        @x = x
        @y = y
        @z = z
        @t = t
    end

    def transform(v)
        @x = @x + v
        @y = "hello " + @y
        @z.sort!
        @t = !@t
        result = {}
        self.instance_variables.each do |var|
            result[var.to_s.delete("@")] = self.instance_variable_get(var)
        end
        return result
    end

end

# Example object
$my_obj = MyClass.new(10, "world", [3, 1, 2], true)
