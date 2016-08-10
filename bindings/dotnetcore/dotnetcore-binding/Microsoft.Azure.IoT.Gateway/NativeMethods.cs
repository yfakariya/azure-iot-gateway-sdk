using System;
using System.Runtime.InteropServices;

namespace Microsoft.Azure.IoT.Gateway
{
    internal static class NativeMethods
    {
        private const string ModuleName = "dotnetcore";
        private const string EntryPoint = "Module_DotNetCoreHost_PublishMessage";
        private const CallingConvention Convention = CallingConvention.Cdecl;

        [DllImport(ModuleName + ".dll", EntryPoint = EntryPoint, CallingConvention = Convention)]
        private static extern bool PublishMessageWinNT(IntPtr messageBusHandle, IntPtr moduleHandle, byte[] source, int sourceLength);

        [DllImport(ModuleName + ".so", EntryPoint = EntryPoint, CallingConvention = Convention)]
        private static extern bool PublishMessageLinux(IntPtr messageBusHandle, IntPtr moduleHandle, byte[] source, int sourceLength);

        [DllImport(ModuleName + ".dylib", EntryPoint = EntryPoint, CallingConvention = Convention)]
        private static extern bool PublishMessageOsx(IntPtr messageBusHandle, IntPtr moduleHandle, byte[] source, int sourceLength);

        public static readonly Func<IntPtr, IntPtr, byte[], int, bool> PublishMessage = InitializePublishMessage();

        private static Func<IntPtr, IntPtr, byte[], int, bool> InitializePublishMessage()
        {
            if ( RuntimeInformation.IsOSPlatform( OSPlatform.Windows ) )
            {
                return PublishMessageWinNT;
            }
            else if ( RuntimeInformation.IsOSPlatform( OSPlatform.OSX ) )
            {
                return PublishMessageOsx;
            }
            else if ( RuntimeInformation.IsOSPlatform( OSPlatform.Linux ) )
            {
                return PublishMessageLinux;
            }
            else
            {
                return ( messageBusHandle, moduleHandle, source, sourceLength ) => { throw new PlatformNotSupportedException( RuntimeInformation.OSDescription ); };
            }
        }
    }
}
