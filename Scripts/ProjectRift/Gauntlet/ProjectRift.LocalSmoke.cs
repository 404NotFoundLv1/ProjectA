using System;
using System.IO;
using Gauntlet;
using UnrealBuildTool;

namespace ProjectRift
{
    /// <summary>
    /// Runs the deterministic ProjectRift lobby-to-rift-to-lobby Development smoke loop.
    /// Packaging is intentionally performed by the PowerShell pipeline before this node runs.
    /// </summary>
    public sealed class LocalSmoke : UnrealTestNode<UnrealTestConfiguration>
    {
        private const int TotalTimeoutSeconds = 180;

        public LocalSmoke(UnrealTestContext context)
            : base(context)
        {
        }

        public override UnrealTestConfiguration GetConfiguration()
        {
            UnrealTestConfiguration config = base.GetConfiguration();
            UnrealTestRole clientRole = config.RequireRole(UnrealTargetRole.Client);

            string runId = Context.TestParams.ParseValue("RunId", string.Empty);
            string userDir = Context.TestParams.ParseValue("UserDir", string.Empty);
            if (!Guid.TryParse(runId, out _) || string.IsNullOrWhiteSpace(userDir) || !Path.IsPathFullyQualified(userDir))
            {
                throw new InvalidOperationException("ProjectRift.LocalSmoke requires a valid RunId GUID and an absolute UserDir.");
            }

            string normalizedRunId = Guid.Parse(runId).ToString("D");
            string expectedSuffix = Path.Combine("Saved", "Automation", "Gauntlet", normalizedRunId, "UserDir");
            string fullUserDir = Path.GetFullPath(userDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            if (!fullUserDir.EndsWith(expectedSuffix, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException("ProjectRift.LocalSmoke UserDir must identify the current ProjectA Saved/Automation/Gauntlet run.");
            }
            DirectoryInfo? runDirectory = Directory.GetParent(fullUserDir);
            DirectoryInfo? gauntletDirectory = runDirectory?.Parent;
            DirectoryInfo? automationDirectory = gauntletDirectory?.Parent;
            DirectoryInfo? savedDirectory = automationDirectory?.Parent;
            DirectoryInfo? projectDirectory = savedDirectory?.Parent;
            if (automationDirectory == null || savedDirectory == null || projectDirectory == null
                || !automationDirectory.Name.Equals("Automation", StringComparison.OrdinalIgnoreCase)
                || !savedDirectory.Name.Equals("Saved", StringComparison.OrdinalIgnoreCase)
                || !File.Exists(Path.Combine(projectDirectory.FullName, "ProjectA.uproject")))
            {
                throw new InvalidOperationException("ProjectRift.LocalSmoke could not prove that UserDir belongs to ProjectA.");
            }

            clientRole.MapOverride = "/Game/ProjectRift/Maps/L_ShipLobby";
            clientRole.Controllers.Add("PRLocalSmokeGauntletController");
            clientRole.CommandLineParams.Add("ProjectRiftGauntletSmoke", runId);
            clientRole.CommandLineParams.Add("ProjectRiftGauntletAutomationRoot", automationDirectory.FullName);
            // RunUnreal owns the final role -userdir and appends its isolated device cache path.
            // The explicit UserDir parameter is still validated as a fail-closed runner contract,
            // but adding a second role argument would be overridden by Gauntlet itself.
            clientRole.CommandLineParams.Add("nullrhi");
            clientRole.CommandLineParams.Add("nosound");
            clientRole.CommandLineParams.Add("unattended");
            clientRole.CommandLineParams.Add("stdout");
            clientRole.CommandLineParams.Add("FullStdOutLogOutput");

            config.MaxDuration = TotalTimeoutSeconds;
            return config;
        }
    }
}
