using System;

namespace Microsoft.Azure.IoT.Gateway.Hosting
{
    /// <summary>Represents HostManager API result.</summary>
    public enum HostManagerResult
    {
        /// <summary>Success.</summary>
        Success = 0,

        /// <summary>Some input argument(s) (mainly pointer) are invalid.</summary>
        InvalidArgument = 1,

        /// <summary>Failed to load module assembly. The configuration may wrong.</summary>
        AssemblyLoadFail = 2,

        /// <summary>Failed to get module type from the assembly. The configuration may wrong.</summary>
        TypeLoadFail = 3,

        /// <summary>Failed to instantiate module instance due to constructor failure.</summary>
        ModuleInstatiationFail = 4,

        /// <summary>The module does not implement required interface.</summary>
        InvalidModuleType = 5,

        /// <summary>The module throws exception.</summary>
        ModuleFail = 6,

        /// <summary>An unexpected exception is thrown. This may indicate fatal runtime error.</summary>
        UnexpectedError = -1
    }
}