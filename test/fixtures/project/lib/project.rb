# frozen_string_literal: true

require_relative "project/version"

Proj::Foo = Class.new
module Project
  class Error < StandardError
    [12]
  end

  class Job
    def test
    end

    class NestedJob

    end
  end

  class Hello::Nested::Super
  end
  # Your code goes here...
end

module SuperModule
end
